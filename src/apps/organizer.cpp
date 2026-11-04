#include "pda/workspace.h"
#include "pda/datetime.h"
#include "pda/ui_style.h"
#include <algorithm>
#include <cstdlib>
#include <stdio.h>

namespace pda {
std::string Workspace::itemTitle(uint32_t id) const {
    const auto &s = sys_.state;
    if (app_ == AppId::Notes) { auto *x = findById(s.notes, id); if (x) return x->title; }
    if (app_ == AppId::Contacts) { auto *x = findById(s.contacts, id); if (x) return x->name; }
    if (app_ == AppId::Agenda || app_ == AppId::Calendar) { auto *x = findById(s.appointments, id); if (x) return x->title; }
    if (app_ == AppId::Sheet) { auto *x = findById(s.sheets, id); if (x) return x->title; }
    if (app_ == AppId::Draw) { auto *x = findById(s.sketches, id); if (x) return x->title; }
    return {};
}
void Workspace::list() {
    form_ = Form::None;
    auto &s = sys_.state;
    button(body_, "Novo", 0, 0, 62, Command::NewItem);
    std::string heading = search_.empty() ? "Digite para buscar" : "Busca: " + search_;
    if (app_ == AppId::Calendar) heading = formatCivil(toUtc(fromDateKey(s.session.day), 0), 0, false);
    if (app_ == AppId::Calendar) { button(body_, "Mês", 70, 0, 52, Command::CalMonth); label(body_, heading, 133, 5, 162); }
    else label(body_, heading, 73, 5, 220);
    auto *listObject = lv_obj_create(body_);
    lv_obj_set_pos(listObject, 0, 32); lv_obj_set_size(listObject, 302, style::bodyHeight - 44);
    lv_obj_set_style_pad_all(listObject, 2, 0); lv_obj_set_style_border_width(listObject, 0, 0);
    lv_obj_set_style_bg_opa(listObject, LV_OPA_TRANSP, 0); lv_obj_set_scroll_dir(listObject, LV_DIR_VER);
    std::vector<std::pair<uint32_t, std::string>> items;
    const std::string query = lowerAscii(search_);
    auto add = [&](uint32_t id, const std::string &title, const std::string &suffix) {
        const std::string name = title.empty() ? "Sem título" : title;
        if (lowerAscii(name + suffix).find(query) != std::string::npos) items.push_back({id, name + suffix});
    };
    if (app_ == AppId::Notes) for (const auto &x : s.notes) add(x.id, x.title, "");
    else if (app_ == AppId::Contacts) for (const auto &x : s.contacts) add(x.id, x.name, x.phone.empty() ? "" : "  " + x.phone);
    else if (app_ == AppId::Sheet) for (const auto &x : s.sheets) add(x.id, x.title, "");
    else if (app_ == AppId::Draw) for (const auto &x : s.sketches) add(x.id, x.title, "");
    else {
        std::vector<const Appointment *> sorted;
        for (const auto &event : s.appointments)
            if (app_ != AppId::Calendar || dateKey(fromUtc(event.start, s.preferences.timezoneMinutes)) == s.session.day) sorted.push_back(&event);
        std::sort(sorted.begin(), sorted.end(), [](const Appointment *a, const Appointment *b) { return a->start < b->start; });
        for (const auto *x : sorted) add(x->id, x->title, "  " + formatCivil(x->start, s.preferences.timezoneMinutes));
    }
    for (size_t i = 0; i < items.size(); ++i) button(listObject, items[i].second, 0, int(i) * 35, 285, Command::OpenItem, int(items[i].first), 30);
    if (items.empty()) label(listObject, "Nenhum item. Selecione Novo.", 0, 10, 280);
}
void Workspace::newItem() {
    auto &s = sys_.state;
    const bool full = (app_ == AppId::Notes && s.notes.size() >= limits::notes) ||
        (app_ == AppId::Contacts && s.contacts.size() >= limits::contacts) ||
        ((app_ == AppId::Agenda || app_ == AppId::Calendar) && s.appointments.size() >= limits::events) ||
        (app_ == AppId::Draw && s.sketches.size() >= limits::sketches) ||
        (app_ == AppId::Sheet && s.sheets.size() >= limits::sheets);
    if (full) { message("Limite de itens atingido. Exporte e remova um item para criar outro."); return; }
    const uint32_t id = s.allocateId(); if (!id) { message("Limite de identificadores atingido."); return; }
    if (app_ == AppId::Notes) { Note x; x.id = id; x.title = "Nova nota"; s.notes.push_back(x); }
    else if (app_ == AppId::Contacts) { Contact x; x.id = id; x.name = "Novo contato"; s.contacts.push_back(x); }
    else if (app_ == AppId::Draw) { Sketch x; x.id = id; x.title = "Desenho " + std::to_string(s.sketches.size() + 1); s.sketches.push_back(std::move(x)); }
    else if (app_ == AppId::Sheet) { Sheet x; x.id = id; x.title = "Planilha " + std::to_string(s.sheets.size() + 1); s.sheets.push_back(x); }
    else {
        Appointment x; x.id = id; x.title = "Novo compromisso";
        CivilTime date = app_ == AppId::Calendar ? fromDateKey(s.session.day) :
            sys_.clockValid() ? fromUtc(sys_.utc(), s.preferences.timezoneMinutes) : fromDateKey(s.session.day);
        date.hour = 9; date.minute = date.second = 0; x.start = toUtc(date, s.preferences.timezoneMinutes);
        s.appointments.push_back(x);
    }
    sys_.changed(true); openItem(id);
}
void Workspace::openItem(uint32_t id) {
    auto &v = sys_.state.session; v.document = id; v.selected = 0; v.draft.clear(); v.field = 0; v.caret = 0;
    if (app_ == AppId::Notes) editor(Form::Note, false);
    else if (app_ == AppId::Contacts) editor(Form::Contact, false);
    else if (app_ == AppId::Agenda || app_ == AppId::Calendar) editor(Form::Event, false);
    else { v.view = app_ == AppId::Draw ? 3 : 4; v.editing = false; v.form = 0; render(); }
    sys_.changed(true);
}
template<class T> static void eraseId(std::vector<T> &items, uint32_t id) {
    items.erase(std::remove_if(items.begin(), items.end(), [id](const T &x) { return x.id == id; }), items.end());
}
void Workspace::deleteItem() {
    auto &s = sys_.state; const uint32_t id = s.session.document;
    if (app_ == AppId::Notes) eraseId(s.notes, id);
    else if (app_ == AppId::Contacts) eraseId(s.contacts, id);
    else if (app_ == AppId::Agenda || app_ == AppId::Calendar) eraseId(s.appointments, id);
    else if (app_ == AppId::Draw) eraseId(s.sketches, id);
    else if (app_ == AppId::Sheet) eraseId(s.sheets, id);
    s.session.document = 0; s.session.view = 0; s.session.form = 0; s.session.draft.clear(); s.session.selected = 0;
    modal_ = Modal::None; sys_.changed(true); render();
}
void Workspace::calendar() {
    auto &v = sys_.state.session; contentFocus_ = true;
    CivilTime date = fromDateKey(v.day);
    button(body_, "<", 0, 0, 28, Command::CalPrevious);
    char month[32]; snprintf(month, sizeof(month), "%02d / %04d", date.month, date.year);
    label(body_, month, 37, 5, 125);
    button(body_, ">", 164, 0, 28, Command::CalNext);
    button(body_, "Hoje", 204, 0, 85, Command::CalToday);
    const char *days[] = {"D", "S", "T", "Q", "Q", "S", "S"};
    for (int i = 0; i < 7; ++i) label(body_, days[i], i * 42 + 12, 31, 30);
    CivilTime first = date; first.day = 1; const int start = weekDay(first);
    for (int day = 1; day <= daysInMonth(date.year, date.month); ++day) {
        const int index = start + day - 1; CivilTime candidate = date; candidate.day = day;
        bool hasEvent = false;
        for (const auto &event : sys_.state.appointments)
            if (dateKey(fromUtc(event.start, sys_.state.preferences.timezoneMinutes)) == dateKey(candidate)) hasEvent = true;
        const std::string text = std::to_string(day) + (hasEvent ? "*" : "");
        auto *cell = button(body_, text, index % 7 * 42, 47 + index / 7 * 19, 37, Command::CalDay, int(dateKey(candidate)), 17);
        lv_obj_set_style_text_font(cell, &lv_font_montserrat_12, 0);
        if (day == date.day) { v.selected = int16_t(focusables_.size() - 1); lv_group_focus_obj(cell); }
    }
}
}
