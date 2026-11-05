#include "pda/workspace.h"
#include "pda/ui_style.h"
#include "pda/expression.h"
#include "pda/datetime.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace pda {
void Workspace::grid() {
    auto &v = sys_.state.session; const auto *sheet = findById(sys_.state.sheets, v.document);
    if (!sheet) return;
    contentFocus_ = true;
    button(body_, "Lista", 0, 0, 48, Command::BackList);
    button(body_, "Nome", 55, 0, 58, Command::Rename);
    button(body_, "Excluir", 121, 0, 67, Command::DeleteItem);
    button(body_, "Ajuda", 197, 0, 92, Command::Help);
    const std::string address = std::string(1, char('A' + v.column)) + std::to_string(v.row + 1);
    auto *source = label(body_, address + "  " + sheet->get(v.row, v.column), 0, 31, 292);
    lv_label_set_long_mode(source, LV_LABEL_LONG_DOT); lv_obj_set_height(source, 18);
    const int firstRow = v.row / 5 * 5, firstColumn = v.column / 4 * 4;
    for (int column = firstColumn; column < firstColumn + 4 && column < limits::columns; ++column)
        label(body_, std::string(1, char('A' + column)), 29 + (column - firstColumn) * 66, 51, 60);
    Evaluator evaluator(sheet);
    for (int row = firstRow; row < firstRow + 5 && row < limits::rows; ++row) {
        label(body_, std::to_string(row + 1), 0, 73 + (row - firstRow) * 19, 27);
        for (int column = firstColumn; column < firstColumn + 4 && column < limits::columns; ++column) {
            const Value value = evaluator.cell(row, column);
            const std::string text = value.kind == ValueKind::Text ? sheet->get(row, column) : value.display();
            auto *cell = button(body_, text, 28 + (column - firstColumn) * 66, 71 + (row - firstRow) * 19,
                62, Command::GridCell, row * 26 + column, 18);
            if (row == v.row && column == v.column) { v.selected = int16_t(focusables_.size() - 1); lv_group_focus_obj(cell); }
        }
    }
}
void Workspace::canvas() {
    auto &v = sys_.state.session; contentFocus_ = true;
    button(body_, "Lista", 0, 0, 43, Command::BackList);
    button(body_, "Nome", 49, 0, 53, Command::Rename);
    button(body_, v.pen ? "Caneta*" : "Caneta", 108, 0, 62, Command::Pen);
    button(body_, v.eraser ? "Borr.*" : "Borr.", 176, 0, 48, Command::Eraser);
    button(body_, "Limpar", 230, 0, 62, Command::ClearCanvas);
    canvasPixels_.resize(limits::sketchWidth * limits::sketchHeight);
    canvas_ = lv_canvas_create(body_); lv_obj_set_pos(canvas_, 0, 32);
    lv_canvas_set_buffer(canvas_, canvasPixels_.data(), limits::sketchWidth, limits::sketchHeight, LV_IMG_CF_TRUE_COLOR);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(canvas_, canvasTouch, LV_EVENT_ALL, this);
    tags_.push_back({Command::Pen, 0}); lv_obj_set_user_data(canvas_, &tags_.back());
    lv_group_add_obj(focus_, canvas_); focusables_.push_back(canvas_); v.selected = int16_t(focusables_.size() - 1);
    cursor_ = lv_obj_create(canvas_); lv_obj_set_size(cursor_, 7, 7);
    lv_obj_set_style_radius(cursor_, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_opa(cursor_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cursor_, 1, 0); lv_obj_set_style_border_color(cursor_, lv_color_hex(0xF05A25), 0);
    lv_obj_clear_flag(cursor_, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(cursor_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_OFF);
    refreshCanvas();
}
void Workspace::refreshCanvas() {
    if (!canvas_) return;
    const auto &v = sys_.state.session; const auto *drawing = findById(sys_.state.sketches, v.document); if (!drawing) return;
    for (int y = 0; y < limits::sketchHeight; ++y)
        for (int x = 0; x < limits::sketchWidth; ++x)
            canvasPixels_[y * limits::sketchWidth + x] = drawing->pixel(x, y) ? lv_color_hex(style::ink) : lv_color_hex(0xFFFFFF);
    if (cursor_) lv_obj_set_pos(cursor_, v.x - 3, v.y - 3);
    lv_obj_invalidate(canvas_);
}
void Workspace::drawLine(int x, int y, bool touch) {
    auto &v = sys_.state.session; auto *drawing = findById(sys_.state.sketches, v.document);
    if (!drawing) return;
    if (touch || v.pen) drawing->line(touch && !touchDrawing_ ? x : v.x, touch && !touchDrawing_ ? y : v.y,
        x, y, !v.eraser, v.eraser ? 2 : 0);
    v.x = int16_t(x); v.y = int16_t(y); sys_.changed(); refreshCanvas();
}
void Workspace::canvasTouch(lv_event_t *event) {
    auto *self = static_cast<Workspace *>(lv_event_get_user_data(event));
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) { self->touchDrawing_ = false; return; }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
    lv_indev_t *input = lv_indev_get_act(); if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) return;
    lv_point_t point; lv_indev_get_point(input, &point); lv_area_t area; lv_obj_get_coords(self->canvas_, &area);
    const int x = std::max(0, std::min(limits::sketchWidth - 1, int(point.x - area.x1)));
    const int y = std::max(0, std::min(limits::sketchHeight - 1, int(point.y - area.y1)));
    self->drawLine(x, y, true); self->touchDrawing_ = true;
}
void Workspace::clock() {
    clockLabel_ = label(body_, "", 0, 0, 294);
    stopwatchLabel_ = label(body_, "", 0, 48, 173);
    timerLabel_ = label(body_, "", 0, 94, 173);
    button(body_, sys_.state.stopwatchRunning ? "Pausa" : "Iniciar", 176, 50, 60, Command::Stopwatch);
    button(body_, "Zerar", 243, 50, 49, Command::ResetStopwatch);
    button(body_, sys_.state.timerRunning ? "Pausa" : "Iniciar", 176, 96, 60, Command::ToggleTimer);
    button(body_, "Zerar", 243, 96, 49, Command::ResetTimer);
    button(body_, "Ajustar hora", 0, 136, 138, Command::SetClock);
    button(body_, "Definir timer", 150, 136, 142, Command::SetTimer);
    refreshClock();
}
static std::string duration(uint64_t milliseconds, bool tenths) {
    const uint64_t seconds = milliseconds / 1000;
    char text[64];
    if (tenths) snprintf(text, sizeof(text), "%02lu:%02lu:%02lu.%u", static_cast<unsigned long>(seconds / 3600),
        static_cast<unsigned long>(seconds / 60 % 60), static_cast<unsigned long>(seconds % 60), unsigned(milliseconds / 100 % 10));
    else snprintf(text, sizeof(text), "%02lu:%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
        static_cast<unsigned long>(seconds / 60 % 60), static_cast<unsigned long>(seconds % 60));
    return text;
}
void Workspace::refreshClock() {
    if (!clockLabel_) return;
    std::string text;
    if (sys_.clockValid()) {
        text = formatCivil(sys_.utc(), sys_.state.preferences.timezoneMinutes);
        const auto date = fromUtc(sys_.utc(), sys_.state.preferences.timezoneMinutes);
        char seconds[8]; snprintf(seconds, sizeof(seconds), ":%02d", date.second); text += seconds;
    } else text = "Hora não ajustada\nAjuste para ativar lembretes.";
    if (strcmp(lv_label_get_text(clockLabel_), text.c_str())) lv_label_set_text(clockLabel_, text.c_str());
    const auto stopwatch = "Cronômetro\n" + duration(sys_.state.stopwatchMs, true);
    const auto timer = "Temporizador\n" + duration(sys_.state.timerMs, false);
    if (strcmp(lv_label_get_text(stopwatchLabel_), stopwatch.c_str())) lv_label_set_text(stopwatchLabel_, stopwatch.c_str());
    if (strcmp(lv_label_get_text(timerLabel_), timer.c_str())) lv_label_set_text(timerLabel_, timer.c_str());
}
void Workspace::calculator() {
    form_ = Form::Calculator; auto &v = sys_.state.session;
    if (v.draft.size() != 1) { v.draft = {sys_.state.calculator}; v.field = 0; v.editing = true; }
    v.form = uint8_t(Form::Calculator); v.view = 6;
    button(body_, "Calcular", 0, 0, 90, Command::SaveForm);
    button(body_, "Símbolos", 100, 0, 90, Command::Symbols);
    button(body_, "Ajuda", 201, 0, 90, Command::Help);
    formFields({"Expressão: + - * / ( )"}, v.draft, {false});
    lv_textarea_set_cursor_pos(fields_[0], int32_t(v.caret));
    if (v.editing) fieldFocus(0, true);
    const Value value = Evaluator().expression(sys_.state.calculator);
    label(body_, sys_.state.calculator.empty() ? "Digite uma expressão e pressione Enter." : "= " + value.display(), 0, 115, 290);
}
void Workspace::settings() {
    std::string status = sys_.store.description();
    if (!sys_.store.error().empty()) status += "\n" + sys_.store.error();
    status += "\nVersão salva: " + std::to_string(sys_.store.generation());
    auto *text = label(body_, status, 0, 0, 290);
    lv_obj_update_layout(text); const int y = lv_obj_get_height(text) + 8;
    button(body_, "Tela, economia e fuso", 0, y, 290, Command::EditSettings);
    button(body_, std::string("Som: ") + (sys_.state.preferences.sound ? "ligado" : "desligado"), 0, y + 35, 290, Command::Sound);
    button(body_, "Testar som", 0, y + 70, 290, Command::TestSound);
    button(body_, std::string("Composição _- para =: ") + (sys_.state.preferences.composeSymbols ? "ligada" : "desligada"), 0, y + 105, 290, Command::Composition);
    button(body_, "Salvar agora", 0, y + 140, 290, Command::SaveNow);
    button(body_, "Exportar cópia para SD", 0, y + 175, 290, Command::Export);
    if (!sys_.store.ready()) button(body_, "Preparar memória (confirmar)", 0, y + 210, 290, Command::Initialize);
    else label(body_, "Memória pronta. Atualizar o firmware preserva os dados.", 0, y + 210, 290);
}
}
