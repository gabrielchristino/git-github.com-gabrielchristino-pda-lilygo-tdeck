#include "pda/workspace.h"
#include "pda/ui_style.h"
#include "pda/datetime.h"
#include "pda/expression.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace pda {
void Workspace::formFields(const std::vector<std::string> &names, const std::vector<std::string> &values, const std::vector<bool> &multiline) {
    multiline_ = multiline;
    int y = 37;
    for (size_t i = 0; i < names.size(); ++i) {
        label(body_, names[i], 0, y, 290); y += 19;
        auto *area = lv_textarea_create(body_);
        lv_obj_set_pos(area, 0, y); lv_obj_set_size(area, 292, multiline[i] ? 97 : 33);
        lv_obj_set_style_pad_all(area, 4, 0); lv_obj_set_style_radius(area, 3, 0);
        lv_obj_set_style_bg_color(area, lv_color_hex(style::field), 0);
        lv_obj_set_style_text_color(area, lv_color_hex(style::ink), 0);
        lv_obj_set_style_text_font(area, &pda_font, 0);
        lv_obj_set_style_border_color(area, lv_color_hex(style::muted), 0);
        lv_obj_set_style_border_width(area, 1, 0);
        lv_obj_set_style_outline_width(area, 0, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(area, 1, LV_STATE_EDITED);
        lv_obj_set_style_border_color(area, lv_color_hex(style::selected), LV_STATE_EDITED);
        if (form_ == Form::Note && !multiline[i]) {
            lv_obj_set_style_border_side(area, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_color(area, lv_color_hex(style::line), 0);
            lv_obj_set_style_radius(area, 0, 0);
        }
        lv_textarea_set_one_line(area, !multiline[i]);
        lv_textarea_set_max_length(area, multiline[i] && form_ == Form::Note ? 4096 : form_ == Form::Cell || form_ == Form::Calculator ? 256 : multiline[i] ? 256 : 64);
        lv_textarea_set_text(area, values[i].c_str());
        tags_.push_back({Command::Field, int(i)}); lv_obj_set_user_data(area, &tags_.back());
        lv_obj_add_event_cb(area, clicked, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(area, changed, LV_EVENT_VALUE_CHANGED, this);
        lv_group_add_obj(focus_, area); focusables_.push_back(area); fields_.push_back(area);
        y += multiline[i] ? 106 : 43;
    }
}
void Workspace::editor(Form form, bool restoreDraft) {
    if (!rendering_) checkpoint();
    const bool alreadyRendering = rendering_; rendering_ = true; clear(); form_ = form;
    auto &s = sys_.state; auto &v = s.session;
    std::vector<std::string> names, values; std::vector<bool> multiline;
    if (form == Form::Note) {
        auto *x = findById(s.notes, v.document); if (!x) { v.view = 0; rendering_ = alreadyRendering; render(); return; }
        names = {"Título", "Texto"}; values = {x->title, x->text}; multiline = {false, true};
    } else if (form == Form::Contact) {
        auto *x = findById(s.contacts, v.document); if (!x) { v.view = 0; rendering_ = alreadyRendering; render(); return; }
        names = {"Nome", "Telefone", "Email", "Observações"}; values = {x->name, x->phone, x->email, x->detail}; multiline = {false,false,false,true};
    } else if (form == Form::Event) {
        auto *x = findById(s.appointments, v.document); if (!x) { v.view = 0; rendering_ = alreadyRendering; render(); return; }
        names = {"Título", "Início: DD/MM/AAAA HH:MM", "Fim (opcional)", "Lembrete: minutos antes (-1 desliga)", "Notas"};
        values = {x->title, formatCivil(x->start, s.preferences.timezoneMinutes), x->end ? formatCivil(x->end, s.preferences.timezoneMinutes) : "", std::to_string(x->reminderMinutes), x->detail};
        if (x->draft.size() == 5) values = x->draft;
        multiline = {false,false,false,false,true};
    } else if (form == Form::Cell) {
        auto *x = findById(s.sheets, v.document); if (!x) { v.view = 0; rendering_ = alreadyRendering; render(); return; }
        names = {std::string("Célula ") + char('A' + v.column) + std::to_string(v.row + 1)};
        values = {x->get(v.row, v.column)}; multiline = {false};
    } else if (form == Form::Rename) { names = {"Nome"}; values = {itemTitle(v.document)}; multiline = {false}; }
    else if (form == Form::Clock) {
        names = {"Data e hora: DD/MM/AAAA HH:MM"};
        values = {formatCivil(sys_.clockValid() ? sys_.utc() : s.lastKnownUtc ? s.lastKnownUtc : 946695600, s.preferences.timezoneMinutes)}; multiline = {false};
    } else if (form == Form::Timer) { names = {"Duração em minutos (até 525600)"}; values = {std::to_string(std::max<uint64_t>(1, s.timerMs / 60000))}; multiline = {false}; }
    else if (form == Form::Settings) {
        names = {"Brilho (1 a 16)", "Reduzir brilho após segundos (0 desliga)", "Apagar tela após segundos (0 desliga)", "Fuso: minutos em relação a UTC"};
        values = {std::to_string(s.preferences.brightness),std::to_string(s.preferences.dimSeconds),std::to_string(s.preferences.offSeconds),std::to_string(s.preferences.timezoneMinutes)};
        multiline = {false,false,false,false};
    }
    if (values.empty()) { rendering_ = alreadyRendering; return; }
    if (!restoreDraft || v.draft.size() != values.size()) {
        v.draft = values; v.field = form == Form::Note ? 1 : 0; v.caret = 0; v.editing = true;
    }
    v.view = 1; v.form = uint8_t(form);
    button(body_, "Salvar", 0, 0, 64, Command::SaveForm);
    button(body_, "Símbolos", 73, 0, 81, Command::Symbols);
    if (form == Form::Note || form == Form::Contact || form == Form::Event || form == Form::Rename)
        button(body_, "Excluir", 164, 0, 66, Command::DeleteItem);
    if (form == Form::Note) button(body_, "Lista", 239, 0, 53, Command::BackList);
    formFields(names, v.draft, multiline);
    const size_t index = std::min<size_t>(v.field, fields_.size() - 1);
    field_ = index;
    lv_textarea_set_cursor_pos(fields_[index], restoreDraft ? int32_t(v.caret) : LV_TEXTAREA_CURSOR_LAST);
    if (v.editing) fieldFocus(index, true);
    lv_obj_update_layout(body_); rendering_ = alreadyRendering;
    if (!alreadyRendering) { sys_.changed(true); refreshFooter(); }
}
void Workspace::fieldFocus(size_t index, bool edit) {
    if (index >= fields_.size()) return;
    for (auto *field : fields_) lv_obj_clear_state(field, LV_STATE_EDITED);
    field_ = index; editing_ = edit; cursorNavigation_ = false;
    auto &v = sys_.state.session; v.field = uint8_t(index); v.editing = edit;
    lv_group_focus_obj(fields_[index]);
    if (edit) lv_obj_add_state(fields_[index], LV_STATE_EDITED);
    lv_obj_scroll_to_view(fields_[index], LV_ANIM_OFF);
    v.caret = lv_textarea_get_cursor_pos(fields_[index]);
    if (!rendering_) sys_.changed();
}
void Workspace::changed(lv_event_t *event) {
    auto *self = static_cast<Workspace *>(lv_event_get_user_data(event));
    if (!self->rendering_) self->syncFields();
}
void Workspace::syncFields() {
    if (fields_.empty() || rendering_) return;
    auto &v = sys_.state.session;
    std::vector<std::string> values;
    for (auto *field : fields_) values.emplace_back(lv_textarea_get_text(field));
    if (values == v.draft) return;
    v.draft = std::move(values);
    if (form_ == Form::Note && v.draft.size() == 2) {
        auto *note = findById(sys_.state.notes, v.document);
        if (note) { note->title = v.draft[0]; note->text = v.draft[1]; }
    } else if (form_ == Form::Contact && v.draft.size() == 4) {
        auto *contact = findById(sys_.state.contacts, v.document);
        if (contact) { contact->name = v.draft[0]; contact->phone = v.draft[1]; contact->email = v.draft[2]; contact->detail = v.draft[3]; }
    } else if (form_ == Form::Event && v.draft.size() == 5) {
        auto *event = findById(sys_.state.appointments, v.document); if (event) event->draft = v.draft;
    } else if (form_ == Form::Cell && v.draft.size() == 1) {
        auto *sheet = findById(sys_.state.sheets, v.document); if (sheet) sheet->set(v.row, v.column, v.draft[0]);
    } else if (form_ == Form::Calculator && v.draft.size() == 1) sys_.state.calculator = v.draft[0];
    sys_.changed();
}
void Workspace::insert(const std::string &text) {
    if (field_ >= fields_.size()) return;
    auto *area = fields_[field_];
    if (form_ == Form::Cell) {
        const auto *sheet = findById(sys_.state.sheets, sys_.state.session.document);
        if (sheet && sheet->get(sys_.state.session.row, sys_.state.session.column).empty() && sheet->cells.size() >= limits::cells) {
            message("Limite de 512 células preenchidas. Limpe uma célula antes de preencher outra."); return;
        }
    }
    const size_t currentLength = strlen(lv_textarea_get_text(area));
    const size_t maxBytes = form_ == Form::Note && field_ == 1 ? limits::noteBytes :
        form_ == Form::Contact && field_ == 3 ? 1024 : form_ == Form::Event && field_ == 4 ? 2048 : 256;
    if (currentLength + text.size() > maxBytes) return;
    if (sys_.state.preferences.composeSymbols && text.size() == 1) {
        const char *source = lv_textarea_get_text(area); const uint32_t cursor = lv_textarea_get_cursor_pos(area);
        size_t byte = 0; uint32_t character = 0;
        while (source[byte] && character < cursor) { if ((uint8_t(source[byte]) & 0xC0) != 0x80) ++character; ++byte; }
        // Finish stepping over the final multibyte character before inspecting ASCII.
        while (source[byte] && (uint8_t(source[byte]) & 0xC0) == 0x80) ++byte;
        const char previous = byte ? source[byte - 1] : 0; char replacement = 0;
        const std::string pair = std::string(1, previous) + text;
        if (pair == "_-") replacement = '='; else if (pair == "(+") replacement = '[';
        else if (pair == "+)") replacement = ']'; else if (pair == "(-") replacement = '<';
        else if (pair == "-)") replacement = '>'; else if (pair == "_#") replacement = '^';
        else if (pair == "_/") replacement = '\\'; else if (pair == "-/") replacement = '%';
        if (replacement) { lv_textarea_del_char(area); lv_textarea_add_char(area, uint8_t(replacement)); checkpoint(); return; }
    }
    lv_textarea_add_text(area, text.c_str()); checkpoint();
}
static bool integer(const std::string &text, long minimum, long maximum, long &value) {
    if (text.empty()) return false;
    char *end = nullptr; value = strtol(text.c_str(), &end, 10);
    return end && !*end && value >= minimum && value <= maximum;
}
void Workspace::saveForm() {
    checkpoint(); auto &s = sys_.state; auto &v = s.session; const auto values = v.draft;
    if (form_ == Form::Calculator) {
        const Value result = Evaluator().expression(s.calculator);
        message("Resultado:\n" + result.display()); return;
    }
    if (form_ == Form::Note && values.size() == 2) { v.view = 0; }
    else if (form_ == Form::Contact && values.size() == 4) {
        if (values[0].empty()) { message("Preencha o nome do contato."); return; }
        auto *x = findById(s.contacts, v.document);
        if (x) { x->name = values[0]; x->phone = values[1]; x->email = values[2]; x->detail = values[3]; } v.view = 0;
    } else if (form_ == Form::Event && values.size() == 5) {
        CivilTime start, end; long reminder = 0;
        if (values[0].empty() || !parseCivil(values[1], start) || (!values[2].empty() && !parseCivil(values[2], end)) ||
            !integer(values[3], -1, 10080, reminder)) { message("Confira título, datas e lembrete. Use DD/MM/AAAA HH:MM; -1 desliga o lembrete."); return; }
        const int64_t startUtc = toUtc(start, s.preferences.timezoneMinutes), endUtc = values[2].empty() ? 0 : toUtc(end, s.preferences.timezoneMinutes);
        if (endUtc && endUtc < startUtc) { message("O fim deve ser igual ou posterior ao início."); return; }
        auto *x = findById(s.appointments, v.document);
        if (x) { x->title = values[0]; x->start = startUtc; x->end = endUtc; x->reminderMinutes = int16_t(reminder); x->detail = values[4]; x->alerted = false; x->draft.clear(); } v.view = 0;
    } else if (form_ == Form::Cell && values.size() == 1) {
        auto *x = findById(s.sheets, v.document);
        if (!x || !x->set(v.row, v.column, values[0])) { message("Limite de 512 células preenchidas ou 256 bytes por célula atingido."); return; }
        v.view = 4;
    } else if (form_ == Form::Rename && values.size() == 1) {
        if (values[0].empty()) { message("Preencha o nome."); return; }
        if (app_ == AppId::Draw) { auto *x = findById(s.sketches, v.document); if (x) x->title = values[0]; v.view = 3; }
        else { auto *x = findById(s.sheets, v.document); if (x) x->title = values[0]; v.view = 4; }
    } else if (form_ == Form::Clock && values.size() == 1) {
        CivilTime date;
        if (!parseCivil(values[0], date) || !sys_.setClock(toUtc(date, s.preferences.timezoneMinutes))) { message("Data inválida. Use DD/MM/AAAA HH:MM (2000 a 2099)."); return; }
        v.view = 5;
    } else if (form_ == Form::Timer && values.size() == 1) {
        long minutes = 0;
        if (!integer(values[0], 1, 525600, minutes)) { message("Use minutos inteiros de 1 a 525600."); return; }
        sys_.startTimer(uint64_t(minutes) * 60000); v.view = 5;
    } else if (form_ == Form::Settings && values.size() == 4) {
        long brightness = 0, dim = 0, off = 0, offset = 0;
        if (!integer(values[0], 1, 16, brightness) || !integer(values[1], 0, 3600, dim) || !integer(values[2], 0, 7200, off) ||
            !integer(values[3], -720, 840, offset) || (off && dim && off < dim)) { message("Confira os limites. O tempo para apagar deve ser maior ou igual ao tempo para reduzir brilho."); return; }
        s.preferences.brightness = uint8_t(brightness); s.preferences.dimSeconds = uint16_t(dim); s.preferences.offSeconds = uint16_t(off);
        s.preferences.timezoneMinutes = int16_t(offset); sys_.configure(); v.view = 7;
    } else return;
    v.form = 0; v.editing = false; v.selected = 0; v.draft.clear(); modal_ = Modal::None;
    sys_.changed(true); render();
}
}
