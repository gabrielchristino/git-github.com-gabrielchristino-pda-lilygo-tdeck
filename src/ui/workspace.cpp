#include "pda/workspace.h"
#include "pda/ui_style.h"
#include "pda/datetime.h"
#include "pda/more_games.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

namespace pda {
Workspace::Workspace(System &system, lv_obj_t *body, lv_obj_t *footer, lv_group_t *focus)
    : sys_(system), body_(body), footer_(footer), focus_(focus) {}
Workspace::~Workspace() = default;

lv_obj_t *Workspace::label(lv_obj_t *parent, const std::string &text, int x, int y, int width) {
    auto *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y); lv_obj_set_width(object, width);
    lv_obj_set_style_text_color(object, lv_color_hex(style::ink), 0);
    lv_obj_set_style_text_font(object, &pda_font, 0);
    lv_label_set_text(object, text.c_str());
    return object;
}
lv_obj_t *Workspace::button(lv_obj_t *parent, const std::string &text, int x, int y, int width, Command command, int argument, int height) {
    auto *object = lv_btn_create(parent);
    style::button(object);
    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
    lv_obj_set_style_pad_all(object, 2, 0);
    tags_.push_back({command, argument});
    lv_obj_set_user_data(object, &tags_.back());
    lv_obj_add_event_cb(object, clicked, LV_EVENT_CLICKED, this);
    auto *textObject = lv_label_create(object); lv_obj_set_size(textObject, width - 6, 18);
    lv_label_set_long_mode(textObject, LV_LABEL_LONG_DOT); lv_label_set_text(textObject, text.c_str()); lv_obj_center(textObject);
    lv_group_add_obj(focus_, object); focusables_.push_back(object);
    return object;
}
void Workspace::clicked(lv_event_t *event) {
    auto *self = static_cast<Workspace *>(lv_event_get_user_data(event));
    const auto *tag = static_cast<ButtonTag *>(lv_obj_get_user_data(lv_event_get_target(event)));
    if (tag) { self->pending_ = tag->command; self->argument_ = tag->argument; }
}
void Workspace::clear() {
    lv_group_remove_all_objs(focus_); lv_obj_clean(body_);
    lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    lv_obj_set_style_pad_all(body_, 6, 0);
    lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    fields_.clear(); multiline_.clear(); focusables_.clear(); tags_.clear();
    canvas_ = cursor_ = clockLabel_ = stopwatchLabel_ = timerLabel_ = nullptr;
    mapCanvas_ = nullptr;
    mapQueryLabel_ = nullptr;
    mapPanel_ = mapMenuButton_ = mapNamesButton_ = nullptr;
    editing_ = false; cursorNavigation_ = false; form_ = Form::None; touchDrawing_ = false;
}
void Workspace::markSession(bool urgent) {
    auto *focused = lv_group_get_focused(focus_);
    const auto found = std::find(focusables_.begin(), focusables_.end(), focused);
    if (found != focusables_.end()) sys_.state.session.selected = int16_t(found - focusables_.begin());
    sys_.changed(urgent);
}
void Workspace::open(AppId app, bool resume) {
    app_ = app; modal_ = Modal::None; pending_ = Command::None; search_.clear(); searchEditing_ = false;
    homeRequested_=false;
    if(app==AppId::Map) { mapControlsVisible_=false; contentFocus_=true; mapMoving_=false; mapSearchOpen_=false; mapApproximate_=false; }
    auto &session = sys_.state.session;
    if (!resume) {
        session.document = 0; session.selected = 0; session.field = 0; session.caret = 0;
        session.editing = false; session.form = 0; session.draft.clear();
        session.view = app == AppId::Calendar ? 2 : app == AppId::Clock ? 5 : app == AppId::Calculator ? 6 : app == AppId::Settings ? 7 : app == AppId::Games ? 8 : 0;
        if (app == AppId::Calendar && sys_.clockValid()) session.day = dateKey(fromUtc(sys_.utc(), sys_.state.preferences.timezoneMinutes));
    }
    // Restore persisted coordinates only on entry, preserving live GPS follow
    // and position when dialogs or search rebuild the map screen.
    if (app == AppId::Map) mapRestore();
    session.app = int8_t(app); session.home = false; pauseSnake(); lastGameStep_=sys_.now(); sys_.changed(true); render();
}
void Workspace::render() {
    rendering_ = true; clear();
    auto &v = sys_.state.session;
    if (v.view == 1 && v.form > 0 && v.form <= uint8_t(Form::Calculator)) editor(Form(v.form), true);
    else if (app_ == AppId::Calendar && v.view == 2) calendar();
    else if (app_ == AppId::Clock) clock();
    else if (app_ == AppId::Calculator) calculator();
    else if (app_ == AppId::Settings) settings();
    else if (app_ == AppId::Games) gameScreen();
    else if (app_ == AppId::Map) mapScreen();
    else if (app_ == AppId::Draw && v.view == 3 && findById(sys_.state.sketches, v.document)) canvas();
    else if (app_ == AppId::Sheet && v.view == 4 && findById(sys_.state.sheets, v.document)) grid();
    else { v.view = 0; list(); }
    lv_obj_update_layout(body_);
    if (app_!=AppId::Map && !editing_ && !focusables_.empty()) {
        const int selected = std::min<int>(std::max<int>(0, v.selected), int(focusables_.size()) - 1);
        lv_group_focus_obj(focusables_[selected]); lv_obj_scroll_to_view(focusables_[selected], LV_ANIM_OFF);
    }
    rendering_ = false; refreshFooter();
}
void Workspace::checkpoint() {
    if (!fields_.empty() && modal_ == Modal::None) syncFields();
    if (editing_ && field_ < fields_.size()) {
        sys_.state.session.caret = lv_textarea_get_cursor_pos(fields_[field_]);
        sys_.state.session.field = uint8_t(field_);
    }
}
void Workspace::leave() {
    pauseSnake();
    checkpoint(); fields_.clear(); focusables_.clear(); multiline_.clear();
    canvas_ = cursor_ = clockLabel_ = stopwatchLabel_ = timerLabel_ = nullptr;
    mapCanvas_ = nullptr;
    mapQueryLabel_ = nullptr;
    mapPanel_ = mapMenuButton_ = mapNamesButton_ = nullptr;
    homeRequested_=false;
    pending_ = Command::None; editing_ = false; modal_ = Modal::None;
}
void Workspace::moveFocus(int delta) {
    if (delta > 0) lv_group_focus_next(focus_); else lv_group_focus_prev(focus_);
    auto *focused = lv_group_get_focused(focus_);
    if (focused) lv_obj_scroll_to_view(focused, LV_ANIM_OFF);
    markSession();
}
bool Workspace::consumesHome() {
    if (modal_ == Modal::None) return false;
    if (modal_ == Modal::Notification) sys_.dismissNotification();
    modal_ = Modal::None; render(); return true;
}
void Workspace::handle(const InputEvent &input) {
    InputEvent event = input;
    if (event.action == Action::Home) return;
    if (modal_ != Modal::None) {
        wasdMovement(event);
        if (event.action == Action::Move) {
            const int steps = modal_ == Modal::Symbols && event.y ? 8 : 1;
            for (int i = 0; i < steps; ++i) moveFocus(event.y ? event.y : event.x);
        }
        else if (event.action == Action::Next) moveFocus(1);
        else if (event.action == Action::Activate) {
            auto *focused = lv_group_get_focused(focus_); if (focused) lv_event_send(focused, LV_EVENT_CLICKED, nullptr);
        } else if (event.action == Action::Backspace) { pending_ = Command::Cancel; }
        return;
    }
    auto &v = sys_.state.session;
    if(app_==AppId::Map) { mapHandle(event); return; }
    if (app_ == AppId::Games && v.view == 8) {
        if (v.document) { gameHandle(event); return; }
        if (event.action == Action::Text && event.character >= '0' && event.character <= '9') {
            pending_ = Command::GameSelect; argument_ = event.character=='0'?9:event.character-'1'; return;
        }
    }
    if (event.action == Action::ToggleNavigation) {
        if (editing_) cursorNavigation_ = !cursorNavigation_;
        else if (v.view == 0) searchEditing_ = !searchEditing_;
        refreshFooter(); return;
    }
    if ((canvas_ || v.view == 2 || v.view == 4) && event.action == Action::Text && event.character == 'm') {
        contentFocus_ = !contentFocus_;
        if (!contentFocus_ && !focusables_.empty()) lv_group_focus_obj(focusables_[0]);
        else render();
        refreshFooter(); return;
    }
    if ((editing_ && cursorNavigation_) || (!editing_ && !(v.view == 0 && searchEditing_))) wasdMovement(event);
    if (app_ == AppId::Sheet && v.view == 4 && contentFocus_ && event.action == Action::Text && event.character >= 32 && event.character < 127) {
        editor(Form::Cell, false);
        if (!fields_.empty()) { lv_textarea_set_text(fields_[0], ""); insert(std::string(1, char(event.character))); }
        return;
    }
    if (canvas_ && contentFocus_ && event.action == Action::Move) {
        const int x = std::max(0, std::min(limits::sketchWidth - 1, int(v.x) + std::max(-12, std::min(12, int(event.x))) * 2));
        const int y = std::max(0, std::min(limits::sketchHeight - 1, int(v.y) + std::max(-12, std::min(12, int(event.y))) * 2));
        drawLine(x, y); return;
    }
    if (canvas_ && event.action == Action::Activate && lv_group_get_focused(focus_) == canvas_) {
        pending_ = Command::Pen; return;
    }
    if (app_ == AppId::Calendar && v.view == 2 && contentFocus_ && event.action == Action::Move) {
        const int delta = event.y ? (event.y > 0 ? 7 : -7) : (event.x > 0 ? 1 : -1);
        v.day = dateKey(addDays(fromDateKey(v.day), delta)); v.selected = 0; markSession(); render(); return;
    }
    if (app_ == AppId::Sheet && v.view == 4 && contentFocus_ && event.action == Action::Move) {
        v.row = int16_t(std::max(0, std::min(limits::rows - 1, int(v.row) + (event.y > 0 ? 1 : event.y < 0 ? -1 : 0))));
        v.column = int16_t(std::max(0, std::min(limits::columns - 1, int(v.column) + (event.x > 0 ? 1 : event.x < 0 ? -1 : 0))));
        v.selected = 0; markSession(); render(); return;
    }
    if (editing_ && field_ < fields_.size()) {
        auto *field = fields_[field_];
        if (event.action == Action::Text && !cursorNavigation_ && event.character >= 32 && event.character < 127) insert(std::string(1, char(event.character)));
        else if (event.action == Action::Backspace) lv_textarea_del_char(field);
        else if (event.action == Action::Move) {
            if (event.x < 0) lv_textarea_cursor_left(field); else if (event.x > 0) lv_textarea_cursor_right(field);
            if (event.y < 0) lv_textarea_cursor_up(field); else if (event.y > 0) lv_textarea_cursor_down(field);
        } else if (event.action == Action::Activate) {
            if (event.character && multiline_[field_] && !cursorNavigation_) lv_textarea_add_text(field, "\n");
            else {
                editing_ = false; cursorNavigation_ = false; v.editing = false; lv_obj_clear_state(field, LV_STATE_EDITED);
                if (form_ == Form::Calculator) pending_ = Command::SaveForm;
                else if (form_ == Form::Cell && event.character) pending_ = Command::SaveForm;
                else moveFocus(1);
            }
        } else if (event.action == Action::Next) {
            editing_ = false; cursorNavigation_ = false; v.editing = false; lv_obj_clear_state(field, LV_STATE_EDITED); moveFocus(1);
        }
        checkpoint(); markSession(); refreshFooter(); return;
    }
    if (event.action == Action::Move) moveFocus(event.y ? event.y : event.x);
    else if (event.action == Action::Next) moveFocus(1);
    else if (event.action == Action::Activate) {
        auto *focused = lv_group_get_focused(focus_); if (focused) lv_event_send(focused, LV_EVENT_CLICKED, nullptr);
    } else if (event.action == Action::Text && event.character >= 32 && event.character < 127) {
        auto *focused = lv_group_get_focused(focus_);
        const auto found = std::find(fields_.begin(), fields_.end(), focused);
        if (found != fields_.end()) { fieldFocus(size_t(found - fields_.begin()), true); insert(std::string(1, char(event.character))); }
        else if (v.view == 0) { searchEditing_ = true; search_ += char(event.character); v.selected = 0; render(); }
    } else if (event.action == Action::Backspace && v.view == 0 && !search_.empty()) {
        search_.pop_back(); v.selected = 0; render();
    }
}
void Workspace::tick() {
    if (pending_ != Command::None) {
        const Command command = pending_; const int argument = argument_; pending_ = Command::None;
        checkpoint(); execute(command, argument);
    }
    if (clockLabel_ && uint32_t(sys_.now() - lastClock_) >= 200) { lastClock_ = sys_.now(); refreshClock(); }
    if(mapCanvas_ && mapGpsRevision_!=maps::gps().revision) {
        mapGpsRevision_=maps::gps().revision;
        if(mapView_.follow) mapView_.followGps();
        lv_obj_invalidate(mapCanvas_);
    }
    if(mapCanvas_ && mapMoving_ && uint32_t(sys_.now()-mapLastMotion_)>=350) {
        mapMoving_=false; lv_obj_invalidate(mapCanvas_);
    }
    if(app_==AppId::Games&&sys_.state.session.document==7&&modal_==Modal::None) {
        auto &g=sys_.state.games.slots[6];
        if(!sys_.notification().empty()&&!g.undoStatus) { pauseSnake(); render(); }
        const uint32_t interval=std::max(120,350-int(std::min(28u,g.score))*8);
        if(uint32_t(sys_.now()-lastGameStep_)>=interval) {
            lastGameStep_=sys_.now();
            if(games::snakeStep(g)) { sys_.changed(); render(); }
        }
    }
    refreshFooter();
}
void Workspace::refreshFooter() {
    std::string text;
    if (modal_ != Modal::None) text = "WASD: mover | Enter: escolher";
    else if(app_==AppId::Map) text = contentFocus_ ? "WASD: mover | I/O: zoom | F: GPS" : "Menu | M: mapa | Enter: escolher";
    else if (!sys_.store.ready()) text = "SEM SALVAR: abra Ajustes > Memória";
    else if (!sys_.store.error().empty()) text = "ERRO ao salvar: abra Ajustes";
    else if (app_ == AppId::Games) {
        if (!sys_.state.session.document) text = "WASD/Enter | 1-9/0: jogar | ";
        else if (sys_.state.session.document == 5) text = "A-Z: tentar | Enter: jogos | ";
        else if (sys_.state.session.document == 3) text = "WASD | Enter: abrir | F: marcar | ";
        else if (sys_.state.session.document == 4) text = "WASD | Enter/1-9 | M: jogos | ";
        else if (sys_.state.session.document >= 6) text = "WASD/Enter | M: jogos | ";
        else text = "WASD | U: desfaz | M: jogos | ";
    }
    else if (editing_) text = cursorNavigation_ ? "Cursor WASD | Shift+$: texto | " : "Texto | Shift+$: cursor | ";
    else if (canvas_ || sys_.state.session.view == 2 || sys_.state.session.view == 4)
        text = contentFocus_ ? "WASD: mover | m: menu | " : "WASD: menu | m: conteúdo | ";
    else if (sys_.state.session.view == 0) text = searchEditing_ ? "Busca | Shift+$: navegar | " : "WASD | Shift+$: buscar | ";
    else text = "WASD: mover | Enter: abrir | ";
    if (app_!=AppId::Map && modal_ == Modal::None && sys_.store.ready() && sys_.store.error().empty()) text += sys_.dirty() ? "salvando…" : "salvo";
    if (strcmp(lv_label_get_text(footer_), text.c_str())) lv_label_set_text(footer_, text.c_str());
}
void Workspace::message(const std::string &text, Modal modal) {
    pauseSnake();
    checkpoint(); modal_ = modal; message_ = text;
    rendering_ = true; clear();
    label(body_, text, 0, 0);
    const bool confirm = modal == Modal::ConfirmDelete || modal == Modal::ConfirmClear || modal == Modal::ConfirmInitialize || modal == Modal::ConfirmGameReset || modal == Modal::ConfirmWarehouseLevel;
    auto *ok = button(body_, confirm ? "Confirmar" : "Voltar", 0, 112, 140, confirm ? Command::ConfirmYes : Command::Cancel);
    if (confirm) {
        auto *cancel = button(body_, "Cancelar", 151, 112, 140, Command::Cancel); lv_group_focus_obj(cancel);
    } else lv_group_focus_obj(ok);
    rendering_ = false; refreshFooter();
}
void Workspace::execute(Command command, int argument) {
    auto &s = sys_.state; auto &v = s.session;
    switch (command) {
    case Command::MapZoomIn: case Command::MapZoomOut: case Command::MapGps: case Command::MapReset:
    case Command::MapMenu: case Command::MapLabels: case Command::MapHelp: case Command::MapHome:
        mapCommand(command); break;
    case Command::MapSearch: case Command::MapFind: case Command::MapSearchBack:
        mapCommand(command); break;
    case Command::MapResult:
        if(argument>=0 && size_t(argument)<mapResults_.size()) {
            const auto point=mapResults_[size_t(argument)].point;
            mapView_.restore(point.x,point.y,2); mapSearchOpen_=false;
            mapControlsVisible_=false; contentFocus_=true; mapMoving_=false; mapApproximate_=false;
            mapCheckpoint(); render();
        }
        break;
    case Command::MapAddressResult:
        if(argument>=0 && size_t(argument)<mapAddressResults_.size()) {
            mapApproximatePoint_=mapAddressResults_[size_t(argument)].point; mapApproximate_=true;
            mapView_.restore(mapApproximatePoint_.x,mapApproximatePoint_.y,3); mapSearchOpen_=false;
            mapControlsVisible_=false; contentFocus_=true; mapMoving_=false;
            mapCheckpoint(); render();
        }
        break;
    case Command::NewItem: newItem(); break;
    case Command::OpenItem: openItem(uint32_t(argument)); break;
    case Command::BackList:
        v.view = 0; v.form = 0; v.editing = false; v.draft.clear(); v.selected = 0; search_.clear(); searchEditing_ = false; sys_.changed(true); render(); break;
    case Command::Field: fieldFocus(size_t(argument), true); break;
    case Command::SaveForm: case Command::SaveCell: case Command::SaveSettings: saveForm(); break;
    case Command::DeleteItem: message("Excluir este item? Esta ação será salva.", Modal::ConfirmDelete); break;
    case Command::ClearCanvas: message("Apagar todos os traços deste desenho?", Modal::ConfirmClear); break;
    case Command::Initialize: message("Preparar a memória do PDA? Cópias anteriores do PDA nesta área serão substituídas pelos dados atuais da RAM.", Modal::ConfirmInitialize); break;
    case Command::ConfirmYes:
        if (modal_ == Modal::ConfirmDelete) deleteItem();
        else if (modal_ == Modal::ConfirmClear) {
            auto *drawing = findById(s.sketches, v.document); if (drawing) drawing->pixels.assign(limits::sketchBytes, 0);
            modal_ = Modal::None; sys_.changed(true); render();
        } else if (modal_ == Modal::ConfirmInitialize) {
            const bool ok = sys_.store.initialize(s); if (ok) sys_.changed(true);
            message(ok ? "Memória pronta. Os dados serão salvos automaticamente." : sys_.store.error());
        } else if (modal_ == Modal::ConfirmWarehouseLevel) {
            auto &game=s.games.slots[unsigned(games::Kind::Warehouse)];
            games::reset(games::Kind::Warehouse,game,game.seed*1664525u+1013904223u+sys_.now(),warehouseChoice_);
            modal_=Modal::None; sys_.changed(true); render();
        } else if (modal_ == Modal::ConfirmGameReset) {
            modal_ = Modal::None; gameReset(app_ == AppId::Games && v.document == 4); render();
        }
        break;
    case Command::Cancel:
        if (modal_ == Modal::Notification) sys_.dismissNotification();
        modal_ = Modal::None; render(); break;
    case Command::Symbols: symbols(); break;
    case Command::Symbol: {
        // Stable UTF-8 palette shared by all text editors.
        static const char *values[] = {"0","1","2","3","4","5","6","7","8","9","=","+","-","*","/","(",")",":",";",".",",","%","[","]","_","\\","<",">","á","é","í","ó","ú","â","ê","ô","ã","õ","ç","à","Á","É","Í","Ó","Ú","Â","Ê","Ô","Ã","Õ","Ç","À","@","#","&","!"};
        if (argument >= 0 && size_t(argument) < sizeof(values) / sizeof(values[0])) {
            modal_ = Modal::None; render(); fieldFocus(v.field, true); insert(values[argument]);
        }
        break;
    }
    case Command::CalPrevious: case Command::CalNext: {
        auto date = fromDateKey(v.day); date.month += command == Command::CalNext ? 1 : -1;
        if (date.month == 0) { date.month = 12; --date.year; } if (date.month == 13) { date.month = 1; ++date.year; }
        date.day = std::min(date.day, daysInMonth(date.year, date.month));
        if (validCivil(date)) v.day = dateKey(date);
        sys_.changed(); render(); break;
    }
    case Command::CalToday:
        if (sys_.clockValid()) { v.day = dateKey(fromUtc(sys_.utc(), s.preferences.timezoneMinutes)); sys_.changed(); render(); }
        else message("Ajuste a data e hora no app Relógio para usar Hoje e os lembretes."); break;
    case Command::CalDay:
        v.day = uint32_t(argument); v.view = 0; v.selected = 0; sys_.changed(true); render(); break;
    case Command::CalMonth: v.view = 2; v.form = 0; sys_.changed(true); render(); break;
    case Command::SetClock: editor(Form::Clock, false); break;
    case Command::EditSettings: editor(Form::Settings, false); break;
    case Command::Stopwatch: sys_.toggleStopwatch(); render(); break;
    case Command::ResetStopwatch: s.stopwatchMs = 0; s.stopwatchRunning = false; sys_.changed(true); render(); break;
    case Command::SetTimer: editor(Form::Timer, false); break;
    case Command::ToggleTimer:
        if (s.timerRunning) { s.timerRunning = false; s.timerDeadline = 0; sys_.changed(true); }
        else sys_.startTimer(s.timerMs);
        render(); break;
    case Command::ResetTimer: sys_.startTimer(0); render(); break;
    case Command::Rename: editor(Form::Rename, false); break;
    case Command::Pen: v.pen = !v.pen; sys_.changed(); refreshCanvas(); break;
    case Command::Eraser: v.eraser = !v.eraser; sys_.changed(); refreshCanvas(); break;
    case Command::GridCell:
        v.row = int16_t(argument / 26); v.column = int16_t(argument % 26); editor(Form::Cell, false); break;
    case Command::Export: {
        std::string result; sys_.save(); sys_.exportData(result); message(result.empty() ? "Exportação indisponível." : result); break;
    }
    case Command::Sound: s.preferences.sound = !s.preferences.sound; sys_.changed(true); render(); break;
    case Command::Composition: s.preferences.composeSymbols = !s.preferences.composeSymbols; sys_.changed(true); render(); break;
    case Command::SaveNow: message(sys_.save() ? "Dados salvos e verificados na flash." : sys_.store.error()); break;
    case Command::Help: message("WASD: mover. Enter: editar (WASD vira texto). Fórmulas: =A1+B1, =SUM(A1:A5), AVG, MIN, MAX, COUNT. Separador ; e decimal com ponto. Use Símbolos para = e acentos."); break;
    case Command::GameSelect:
        if (argument >= 0 && argument < int(games::count)) {
            s.games.selected = uint8_t(argument); v.document = uint32_t(argument + 1); v.view = 8; v.selected = 0;
            if (!s.games.slots[argument].started) gameReset();
            sys_.changed(true); render();
        }
        break;
    case Command::GameMenu: pauseSnake(); v.document = 0; v.selected = s.games.selected; sys_.changed(true); render(); break;
    case Command::GameNew: message("Começar de novo? A partida atual deste jogo será substituída.", Modal::ConfirmGameReset); break;
    case Command::GameHelp: gameHelp(); break;
    case Command::GameHelpTab: gameHelp(unsigned(argument)); break;
    case Command::GameWarehouseLevel:
        if(app_==AppId::Games&&v.document==2&&argument>=0&&unsigned(argument)<games::levelCount(games::Kind::Warehouse)) {
            warehouseChoice_=unsigned(argument);
            message("Começar a fase "+std::to_string(argument+1)+"? A partida atual do Sokoban será substituída.",Modal::ConfirmWarehouseLevel);
        }
        break;
    case Command::GameSpecial:
        if (v.document >= 1 && v.document <= games::count && games::special(games::Kind(v.document - 1), s.games.slots[v.document - 1])) { sys_.changed(); render(); }
        break;
    case Command::GameCell:
        if (v.document == 3 || v.document == 4 || v.document == 6 || v.document == 9) {
            const int cells=v.document==6?64:int(s.games.slots[v.document-1].board.size());
            if (argument >= 0 && argument < cells) {
                auto &game = s.games.slots[v.document - 1]; game.cursor = uint8_t(argument);
                if (v.document == 3) games::activate(games::Kind::Mines, game);
                if(v.document==6||v.document==9) games::activate(games::Kind(v.document-1),game);
                sys_.changed(); render();
            }
        }
        break;
    default: break;
    }
}
void Workspace::symbols() {
    if (fields_.empty()) { message("Abra um campo de texto primeiro."); return; }
    checkpoint(); modal_ = Modal::Symbols; rendering_ = true; clear();
    static const char *values[] = {"0","1","2","3","4","5","6","7","8","9","=","+","-","*","/","(",")",":",";",".",",","%","[","]","_","\\","<",">","á","é","í","ó","ú","â","ê","ô","ã","õ","ç","à","Á","É","Í","Ó","Ú","Â","Ê","Ô","Ã","Õ","Ç","À","@","#","&","!"};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        button(body_, values[i], int(i % 8) * 37, int(i / 8) * 22, 33, Command::Symbol, int(i), 19);
    lv_group_focus_obj(focusables_[0]); rendering_ = false;
}
}
