#include "pda/shell.h"
#include "pda/ui_style.h"
#include "pda/datetime.h"
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(pda_font);
namespace pda {
namespace {
const char *appIcon(AppId id) {
    switch (id) {
    case AppId::Agenda: return LV_SYMBOL_LIST;
    case AppId::Calendar: return "31";
    case AppId::Notes: return LV_SYMBOL_EDIT;
    case AppId::Draw: return LV_SYMBOL_IMAGE;
    case AppId::Clock: return LV_SYMBOL_BELL;
    case AppId::Contacts: return LV_SYMBOL_CALL;
    case AppId::Calculator: return LV_SYMBOL_PLUS;
    case AppId::Sheet: return LV_SYMBOL_LIST;
    case AppId::InputTest: return LV_SYMBOL_KEYBOARD;
    case AppId::Settings: return LV_SYMBOL_SETTINGS;
    case AppId::Games: return LV_SYMBOL_PLAY;
    case AppId::Map: return LV_SYMBOL_GPS;
    case AppId::Bluetooth: return LV_SYMBOL_BLUETOOTH;
    case AppId::Chat: return LV_SYMBOL_EDIT;
    }
    return LV_SYMBOL_FILE;
}
}
void Shell::begin(bool touchReady, System &system) {
    system_ = &system; touchReady_ = touchReady;
    focus_ = lv_group_create(); lv_group_set_wrap(focus_, false);
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(style::paper), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(style::ink), 0);
    lv_obj_set_style_text_font(screen, &pda_font, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    auto *bar = lv_obj_create(screen); lv_obj_set_pos(bar, 0, 0); lv_obj_set_size(bar, 320, style::headerHeight);
    lv_obj_set_style_radius(bar, 0, 0); lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(style::line), 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(style::paper), 0); lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    auto *home = lv_btn_create(bar); lv_obj_set_pos(home, 2, 2); lv_obj_set_size(home, 34, 24);
    lv_obj_set_style_radius(home, 4, 0); lv_obj_set_style_shadow_width(home, 0, 0);
    lv_obj_set_style_bg_color(home, lv_color_hex(style::paper), 0);
    lv_obj_set_style_text_color(home, lv_color_hex(style::ink), 0);
    lv_obj_add_event_cb(home, homeClicked, LV_EVENT_CLICKED, this);
    auto *homeLabel = lv_label_create(home); lv_label_set_text(homeLabel, LV_SYMBOL_HOME); lv_obj_center(homeLabel);
    title_ = lv_label_create(bar); lv_obj_set_pos(title_, 40, 5); lv_obj_set_width(title_, 166);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT); lv_obj_set_style_text_color(title_, lv_color_hex(style::ink), 0);
    lv_obj_set_style_text_font(title_, &pda_font, 0);
    time_ = lv_label_create(bar); lv_obj_set_pos(time_, 260, 7); lv_obj_set_width(time_, 52);
    lv_obj_set_style_text_font(time_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_, lv_color_hex(style::muted), 0);
    lv_obj_set_style_text_align(time_, LV_TEXT_ALIGN_RIGHT, 0); lv_label_set_text(time_, "--:--");
    battery_ = lv_label_create(bar); lv_obj_set_pos(battery_, 210, 7); lv_obj_set_width(battery_, 46);
    lv_obj_set_style_text_font(battery_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_, lv_color_hex(style::muted), 0);
    lv_obj_set_style_text_align(battery_, LV_TEXT_ALIGN_RIGHT, 0); lv_label_set_text(battery_, "--%");
    body_ = lv_obj_create(screen); lv_obj_set_pos(body_, 0, style::bodyTop); lv_obj_set_size(body_, 320, style::bodyHeight);
    lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0); lv_obj_set_style_text_color(body_, lv_color_hex(style::ink), 0);
    lv_obj_set_style_text_font(body_, &pda_font, 0); lv_obj_set_style_border_width(body_, 0, 0);
    lv_obj_set_style_pad_all(body_, 6, 0); lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    footer_ = lv_label_create(screen); lv_obj_set_pos(footer_, 7, 219); lv_obj_set_width(footer_, 306);
    lv_label_set_long_mode(footer_, LV_LABEL_LONG_CLIP); lv_obj_set_style_text_font(footer_, &pda_font, 0);
    lv_obj_set_style_text_color(footer_, lv_color_hex(style::muted), 0);
    lv_obj_set_style_border_side(footer_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(footer_, 1, 0);
    lv_obj_set_style_border_color(footer_, lv_color_hex(style::line), 0);
    workspace_.reset(new Workspace(system, body_, footer_, focus_));
    const auto saved = system.state.session;
    if (!saved.home && saved.app >= 0 && size_t(saved.app) < appCount) showApp(apps[saved.app]);
    else showHome();
}
bool Shell::matches(const char *name) const { return lowerAscii(name).find(lowerAscii(search_)) != std::string::npos; }
void Shell::select(int index) {
    if (!visibleCount_) return;
    selected_ = std::max(0, std::min(int(visibleCount_) - 1, index));
    lv_group_focus_obj(buttons_[selected_]); lv_obj_scroll_to_view(buttons_[selected_], LV_ANIM_OFF);
    if (listCount_) {
        const int first = std::min(std::max(0, selected_ - 4), std::max(0, int(visibleCount_) - 5));
        lv_label_set_text_fmt(listCount_, "%d-%d / %u", first + 1, std::min(first + 5, int(visibleCount_)), unsigned(visibleCount_));
    }
}
void Shell::rebuildList() {
    lv_obj_add_flag(body_,LV_OBJ_FLAG_SCROLLABLE);
    listCount_ = nullptr;
    lv_group_remove_all_objs(focus_); lv_obj_clean(body_); lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_AUTO); visibleCount_ = 0;
    auto *heading = lv_label_create(body_); lv_label_set_text(heading, "Pessoal");
    lv_obj_set_pos(heading, 4, 0); lv_obj_set_style_text_font(heading, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(style::muted), 0);
    listCount_ = lv_label_create(body_); lv_obj_set_pos(listCount_, 196, 0); lv_obj_set_width(listCount_, 98);
    lv_obj_set_style_text_font(listCount_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(listCount_, lv_color_hex(style::muted), 0);
    lv_obj_set_style_text_align(listCount_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(listCount_, "0 / 0");
    for (size_t i = 0; i < appCount && visibleCount_ < maxApps; ++i) {
        if (!matches(apps[i].name)) continue;
        auto *button = lv_btn_create(body_); style::button(button);
        lv_obj_set_pos(button, 0, 18 + visibleCount_ * 31); lv_obj_set_size(button, 296, 28);
        lv_obj_set_style_pad_hor(button, 10, 0);
        lv_obj_set_user_data(button, const_cast<AppInfo *>(&apps[i])); lv_obj_add_event_cb(button, appClicked, LV_EVENT_CLICKED, this);
        auto *label = lv_label_create(button); lv_label_set_text(label, apps[i].name); lv_obj_align(label, LV_ALIGN_LEFT_MID, 30, 0);
        auto *icon = lv_label_create(button); lv_label_set_text(icon, appIcon(apps[i].id));
        lv_obj_set_width(icon, 22); lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
        if (apps[i].id == AppId::Calendar) {
            lv_obj_set_style_border_width(icon, 1, 0);
            lv_obj_set_style_border_color(icon, lv_color_hex(style::line), 0);
            lv_obj_set_style_radius(icon, 2, 0);
        }
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
        lv_group_add_obj(focus_, button); buttons_[visibleCount_] = button; visible_[visibleCount_++] = &apps[i];
    }
    if (!visibleCount_) { auto *empty = lv_label_create(body_); lv_obj_set_pos(empty, 4, 24); lv_label_set_text(empty, "Nenhum app encontrado.\nBackspace apaga a busca."); }
    lv_obj_update_layout(body_); select(selected_); updateFooter();
}
void Shell::showHome() {
#ifdef PDA_CHAT
    leaveChat();
#endif
    if (remote_.enable) remote_.enable(false);
    remoteStatus_ = nullptr;
    remotePanel_ = nullptr; remoteMask_ = 0; remoteRebuild_ = false;
    if (workspace_) workspace_->leave();
    mapLayout(false);
    current_ = nullptr; diagnostic_ = nullptr; searchEditing_ = false;
    lv_label_set_text(title_, "Aplicativos");
    if (!system_->state.session.home) { system_->state.session.home = true; system_->changed(true); }
    rebuildList();
}
void Shell::showApp(const AppInfo &app) {
#ifdef PDA_CHAT
    leaveChat();
#endif
    if (remote_.enable && current_ && current_->id == AppId::Bluetooth) remote_.enable(false);
    remoteStatus_ = nullptr;
    listCount_ = nullptr;
    current_ = &app; diagnostic_ = nullptr; lv_label_set_text(title_, app.name);
    lv_obj_add_flag(body_,LV_OBJ_FLAG_SCROLLABLE);
    mapLayout(app.id==AppId::Map || (app.id==AppId::Games && system_->state.session.app==int8_t(app.id) && system_->state.session.document>=1 && system_->state.session.document<=games::count));
    auto &session = system_->state.session;
#ifdef PDA_CHAT
    if (app.id == AppId::Chat) {
        workspace_->leave(); lv_group_remove_all_objs(focus_); lv_obj_clean(body_);
        // Chat is transient. Never persist app=13 or chat text into legacy saves.
        if (!session.home) { session.home = true; system_->changed(true); }
        buildChat();
    }
    else
#endif
    if (app.id == AppId::Bluetooth) {
        workspace_->leave(); lv_group_remove_all_objs(focus_); lv_obj_clean(body_);
        lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
        session.app = int8_t(app.id); session.home = false; session.view = 0; system_->changed(true);
        remoteTab_ = remoteMask_ = remoteSymbols_ = 0; remoteRebuild_ = false;
        remoteManaging_ = remoteForgetConfirm_ = false; remoteEditId_.clear(); remotePeerMessage_.clear();
        remotePeerAction_ = PeerAction::None;
        buildRemotePanel();
        if (remote_.enable) remote_.enable(true);
        refreshRemote();
    }
    else if (app.id != AppId::InputTest) workspace_->open(app.id, session.app == int8_t(app.id));
    else {
        workspace_->leave(); lv_group_remove_all_objs(focus_); lv_obj_clean(body_); lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
        session.app = int8_t(app.id); session.home = false; session.view = 0; system_->changed(true);
        diagnostic_ = lv_label_create(body_); lv_obj_set_width(diagnostic_, 296);
        lv_obj_set_pos(diagnostic_, 0, 34);
        lv_obj_set_style_text_font(diagnostic_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_line_space(diagnostic_, 0, 0);
        const char *tabs[] = {"Bateria", "GPS", "Entradas"};
        for (unsigned i = 0; i < 3; ++i) {
            auto *tab = diagnosticTabs_[i] = lv_btn_create(body_); style::button(tab);
            lv_obj_set_pos(tab, i * 100, 0); lv_obj_set_size(tab, 96, 27);
            auto *name = lv_label_create(tab); lv_label_set_text(name, tabs[i]); lv_obj_center(name);
            lv_obj_add_event_cb(tab, diagnosticClicked, LV_EVENT_CLICKED, this);
        }
        batteryBar_ = lv_bar_create(body_); lv_obj_set_pos(batteryBar_, 0, 34); lv_obj_set_size(batteryBar_, 296, 8);
        lv_obj_set_style_bg_color(batteryBar_, lv_color_hex(style::line), LV_PART_MAIN);
        lv_obj_set_style_bg_color(batteryBar_, lv_color_hex(style::selected), LV_PART_INDICATOR);
        selectDiagnostic(diagnosticTab_);
        lv_label_set_text(footer_, "1-3/Tab: abas | segure: início");
    }
    updateFooter();
}
void Shell::mapLayout(bool enabled) {
    immersive_=enabled;
    auto *bar=lv_obj_get_parent(title_);
    if(enabled) { lv_obj_add_flag(bar,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(footer_,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_clear_flag(bar,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(footer_,LV_OBJ_FLAG_HIDDEN); }
    if(enabled && current_ && current_->id==AppId::Games && system_->state.session.document!=1 && system_->state.session.document!=2 && system_->state.session.document!=3 && system_->state.session.document!=9 && system_->state.session.document!=10) lv_obj_clear_flag(footer_,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(body_,0,enabled?0:style::bodyTop);
    lv_obj_set_size(body_,320,enabled?240:style::bodyHeight);
    lv_obj_set_style_pad_all(body_,enabled?0:6,0);
    lv_obj_update_layout(body_);
}
void Shell::updateFooter() {
    if (current_) return;
    std::string text;
    if (!system_->store.ready()) text = "Sem salvar: abra Ajustes";
    else if (!system_->store.error().empty()) text = "Erro ao salvar: abra Ajustes";
    else if (searchEditing_) text = std::string("Busca: ") + search_ + " | Shift+$: sair";
    else text = "WASD: mover | Shift+$: buscar";
    if (strcmp(lv_label_get_text(footer_), text.c_str())) lv_label_set_text(footer_, text.c_str());
}
void Shell::appClicked(lv_event_t *event) {
    auto *self = static_cast<Shell *>(lv_event_get_user_data(event));
    auto *app = static_cast<AppInfo *>(lv_obj_get_user_data(lv_event_get_target(event)));
    for (size_t i = 0; i < self->visibleCount_; ++i) if (self->visible_[i] == app) self->selected_ = int(i);
    self->pendingApp_ = app;
}
void Shell::homeClicked(lv_event_t *event) { static_cast<Shell *>(lv_event_get_user_data(event))->pendingHome_ = true; }
void Shell::alertClicked(lv_event_t *event) {
    auto *self = static_cast<Shell *>(lv_event_get_user_data(event)); self->system_->dismissNotification();
}
void Shell::handle(const InputEvent &input) {
    InputEvent event = input;
    ++events_;
    // Notifications consume input before an app can forward it to a remote host.
    if (alert_) { if (event.action == Action::Activate || event.action == Action::Home || event.action == Action::Backspace) system_->dismissNotification(); return; }
#ifdef PDA_CHAT
    if (current_ && current_->id == AppId::Chat) {
        if (event.action == Action::Home) pendingHome_ = true;
        else handleChat(event);
        return;
    }
#endif
    if (current_ && current_->id == AppId::Bluetooth) {
        if (event.action == Action::Home && !event.character) pendingHome_ = true;
        else if (remoteManaging_) handleRemotePeers(event);
        else {
            // Factory keyboard's Shift+$ is byte 0x04; expose the missing '='.
            if (event.action == Action::ToggleNavigation) { event.action = Action::Text; event.character = '='; }
            if (event.character && remote_.key) sendRemoteCharacter(event.character);
            else if (remote_.input) remote_.input(event);
        }
        return;
    }
    if (event.action == Action::Home) {
        if (current_ && current_->id != AppId::InputTest && workspace_->consumesHome()) return;
        pendingHome_ = true; return;
    }
    if (current_) {
        if (diagnostic_) {
            if (event.character >= '1' && event.character <= '3') selectDiagnostic(event.character - '1');
            else if (event.character == 'g' || event.character == 'G') selectDiagnostic(diagnosticTab_ == 1 ? 2 : 1);
            else if (event.character == 'b' || event.character == 'B') selectDiagnostic(0);
            else if (event.action == Action::Next || event.action == Action::Activate) selectDiagnostic((diagnosticTab_ + 1) % 3);
            if (event.action == Action::Move) { lastX_ = event.x; lastY_ = event.y; }
            else if (event.character) lastKey_ = event.character;
            refreshDiagnostic();
        }
        else workspace_->handle(event);
        return;
    }
    if (event.action == Action::ToggleNavigation) { searchEditing_ = !searchEditing_; updateFooter(); return; }
    if (!searchEditing_) wasdMovement(event);
    switch (event.action) {
    case Action::Move: if (event.y || event.x) select(selected_ + ((event.y ? event.y : event.x) > 0 ? 1 : -1)); break;
    case Action::Next: select(selected_ + 1); break;
    case Action::Activate: if (visibleCount_) pendingApp_ = visible_[selected_]; break;
    case Action::Backspace: { const size_t n = strlen(search_); if (n) { search_[n - 1] = 0; selected_ = 0; rebuildList(); } break; }
    case Action::Text: { const size_t n = strlen(search_); if (event.character >= 32 && event.character < 127 && n < sizeof(search_) - 1) { searchEditing_ = true; search_[n] = char(tolower(event.character)); search_[n + 1] = 0; selected_ = 0; rebuildList(); } break; }
    default: break;
    }
}
void Shell::setTrackballStats(const TrackballStats &stats) { ball_ = stats; refreshDiagnostic(); }
void Shell::setGpsDiagnostic(const char *text) {
    snprintf(gpsText_, sizeof(gpsText_), "%s", text); refreshDiagnostic();
}
void Shell::diagnosticClicked(lv_event_t *event) {
    auto *self = static_cast<Shell *>(lv_event_get_user_data(event));
    for (unsigned i = 0; i < 3; ++i)
        if (self->diagnosticTabs_[i] == lv_event_get_target(event)) self->selectDiagnostic(i);
}
void Shell::selectDiagnostic(unsigned tab) {
    diagnosticTab_ = tab % 3;
    for (unsigned i = 0; i < 3; ++i) {
        lv_obj_set_style_bg_color(diagnosticTabs_[i], lv_color_hex(i == diagnosticTab_ ? style::selected : style::paper), 0);
        lv_obj_set_style_text_color(diagnosticTabs_[i], lv_color_hex(i == diagnosticTab_ ? style::onSelected : style::ink), 0);
    }
    lv_obj_set_pos(diagnostic_, 0, diagnosticTab_ == 0 ? 49 : 34);
    lv_obj_set_style_text_font(diagnostic_, diagnosticTab_ == 1 ? &lv_font_montserrat_12 : &pda_font, 0);
    refreshDiagnostic();
}
void Shell::setBatteryStats(const BatteryStats &stats) {
    batteryStats_ = stats;
    setBatteryPercent(stats.valid() ? stats.percent : -1);
    refreshDiagnostic();
}
void Shell::refreshDiagnostic() {
    if (!diagnostic_) return;
    if (diagnosticTab_ == 0 && batteryStats_.valid()) lv_obj_clear_flag(batteryBar_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(batteryBar_, LV_OBJ_FLAG_HIDDEN);
    if (diagnosticTab_ == 0) {
        char text[512];
        const auto &b = batteryStats_;
        if (b.valid()) {
            lv_bar_set_value(batteryBar_, b.percent, LV_ANIM_OFF);
            snprintf(text, sizeof(text),
                "Carga estimada: %d%%\nTensão: %d.%03d V (%d mV)\nMín./máx.: %d.%03d / %d.%03d V\n%lu leituras desde boot | 10 s\nNível: %s (estimado por tensão)\nSaúde/ciclos/USB: não medidos",
                b.percent, b.millivolts / 1000, b.millivolts % 1000, b.millivolts,
                b.minimum / 1000, b.minimum % 1000, b.maximum / 1000, b.maximum % 1000,
                static_cast<unsigned long>(b.samples), b.percent <= 10 ? "crítico" : b.percent <= 20 ? "baixo" : "normal");
        } else snprintf(text, sizeof(text), "Bateria: leitura indisponível\nTensão e porcentagem: --\nNova leitura a cada 10 s.\n\nSaúde / desgaste: não medidos\nCiclos e carga USB: indisponíveis");
        if (strcmp(lv_label_get_text(diagnostic_), text)) lv_label_set_text(diagnostic_, text);
        return;
    }
    if (diagnosticTab_ == 1) {
        if (strcmp(lv_label_get_text(diagnostic_), gpsText_)) lv_label_set_text(diagnostic_, gpsText_);
        return;
    }
    char text[320];
    snprintf(text, sizeof(text),
        "Pulsos reais da bola\nEsq: %lu   Dir: %lu\nCima: %lu   Baixo: %lu\nGPIO1 (esq): %u\nEsq por polling: %lu\nÚltimo X: %d  Y: %d\nTecla: 0x%02X   Touch: %s",
        static_cast<unsigned long>(ball_.left), static_cast<unsigned long>(ball_.right),
        static_cast<unsigned long>(ball_.up), static_cast<unsigned long>(ball_.down), unsigned(ball_.levels & 1),
        static_cast<unsigned long>(ball_.polledLeft), lastX_, lastY_, lastKey_, touchReady_ ? "OK" : "ausente");
    if (strcmp(lv_label_get_text(diagnostic_), text)) lv_label_set_text(diagnostic_, text);
}
void Shell::setBatteryPercent(int percent) {
    char text[8] = "--%";
    if (percent >= 0) snprintf(text, sizeof(text), "%d%%", std::min(percent, 100));
    if (battery_ && strcmp(lv_label_get_text(battery_), text)) lv_label_set_text(battery_, text);
}
void Shell::tick() {
    char clock[8] = "--:--";
    if (system_->clockValid()) {
        const auto local = fromUtc(system_->utc(), system_->state.preferences.timezoneMinutes);
        snprintf(clock, sizeof(clock), "%02d:%02d", local.hour, local.minute);
    }
    if (strcmp(lv_label_get_text(time_), clock)) lv_label_set_text(time_, clock);
    if (pendingHome_) { pendingHome_ = false; pendingApp_ = nullptr; search_[0] = 0; showHome(); system_->save(); }
    else if (pendingApp_) { const AppInfo *app = pendingApp_; pendingApp_ = nullptr; showApp(*app); }
#ifdef PDA_CHAT
    if (current_ && current_->id == AppId::Chat) tickChat();
    else
#endif
    if (current_ && current_->id == AppId::Bluetooth) {
        tickRemotePeers();
        if (remoteRebuild_) { if (remoteManaging_) buildRemotePeers(); else buildRemotePanel(); }
        else refreshRemote();
    }
    else if (current_ && !diagnostic_) {
        workspace_->tick();
        const bool full=current_ && (current_->id==AppId::Map || (current_->id==AppId::Games && system_->state.session.document>=1 && system_->state.session.document<=games::count));
        if(full!=immersive_) mapLayout(full);
        if(workspace_->takeHomeRequest()) { search_[0]=0; showHome(); system_->save(); }
    } else updateFooter();
    if (system_->notification().empty() && alert_) { lv_obj_del(alert_); alert_ = nullptr; }
    else if (alert_) {
        auto *text = lv_obj_get_child(alert_, 0);
        if (strcmp(lv_label_get_text(text), system_->notification().c_str())) lv_label_set_text(text, system_->notification().c_str());
    }
    else if (!system_->notification().empty() && !alert_) {
        alert_ = lv_obj_create(lv_layer_top()); lv_obj_set_size(alert_, 300, 175); lv_obj_center(alert_);
        lv_obj_set_style_bg_color(alert_, lv_color_hex(style::paper), 0); lv_obj_set_style_text_font(alert_, &pda_font, 0);
        lv_obj_set_style_text_color(alert_, lv_color_hex(style::ink), 0);
        auto *text = lv_label_create(alert_); lv_obj_set_width(text, 260); lv_label_set_text(text, system_->notification().c_str());
        auto *ok = lv_btn_create(alert_); lv_obj_set_size(ok, 100, 30); lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, 0);
        auto *textOk = lv_label_create(ok); lv_label_set_text(textOk, "OK"); lv_obj_center(textOk);
        lv_obj_add_event_cb(ok, alertClicked, LV_EVENT_CLICKED, this);
    }
}
}
