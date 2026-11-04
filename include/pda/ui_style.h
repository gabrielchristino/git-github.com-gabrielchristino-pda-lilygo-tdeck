#pragma once
#include <stdint.h>
#include <lvgl.h>
LV_FONT_DECLARE(pda_font);
namespace pda { namespace style {
constexpr uint32_t paper = 0xF3F5EF, ink = 0x233E33, muted = 0x52665B, card = 0xE6ECE3, selected = 0x24563E;
constexpr uint32_t field = paper, line = 0xD0D9D1, onSelected = 0xFFFFFF;
constexpr int headerHeight = 28, bodyTop = 30, bodyHeight = 184;
// Use the bundled symbol font: no bitmaps, new fonts or animations.
inline void button(lv_obj_t *object) {
    lv_obj_remove_style_all(object);
    lv_obj_set_style_radius(object, 3, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_side(object, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(line), 0);
    lv_obj_set_style_border_opa(object, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(object, lv_color_hex(paper), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(selected), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(object, lv_color_hex(ink), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(object, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(object, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(object, lv_color_hex(ink), 0);
    lv_obj_set_style_text_color(object, lv_color_hex(onSelected), LV_STATE_FOCUSED);
    lv_obj_set_style_text_font(object, &pda_font, 0);
}
} }
