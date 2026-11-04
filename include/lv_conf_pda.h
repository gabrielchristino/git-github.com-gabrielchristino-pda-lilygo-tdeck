#pragma once
// Preserve the reference project's display, allocator, fonts and theme. Only
// compile widgets the PDA actually uses; Calendar/Sheet use our own controls.
#include "../lib/lv_conf.h"

#undef LV_USE_ARC
#define LV_USE_ARC 0
#undef LV_USE_ANIMIMG
#define LV_USE_ANIMIMG 0
#undef LV_USE_BTNMATRIX
#define LV_USE_BTNMATRIX 0
#undef LV_USE_CHECKBOX
#define LV_USE_CHECKBOX 0
#undef LV_USE_DROPDOWN
#define LV_USE_DROPDOWN 0
#undef LV_USE_ROLLER
#define LV_USE_ROLLER 0
#undef LV_USE_SLIDER
#define LV_USE_SLIDER 0
#undef LV_USE_SWITCH
#define LV_USE_SWITCH 0
#undef LV_USE_TABLE
#define LV_USE_TABLE 0
#undef LV_USE_CALENDAR
#define LV_USE_CALENDAR 0
#undef LV_USE_CALENDAR_HEADER_ARROW
#define LV_USE_CALENDAR_HEADER_ARROW 0
#undef LV_USE_CALENDAR_HEADER_DROPDOWN
#define LV_USE_CALENDAR_HEADER_DROPDOWN 0
#undef LV_USE_CHART
#define LV_USE_CHART 0
#undef LV_USE_COLORWHEEL
#define LV_USE_COLORWHEEL 0
#undef LV_USE_IMGBTN
#define LV_USE_IMGBTN 0
#undef LV_USE_KEYBOARD
#define LV_USE_KEYBOARD 0
#undef LV_USE_LED
#define LV_USE_LED 0
#undef LV_USE_LIST
#define LV_USE_LIST 0
#undef LV_USE_MENU
#define LV_USE_MENU 0
#undef LV_USE_METER
#define LV_USE_METER 0
#undef LV_USE_MSGBOX
#define LV_USE_MSGBOX 0
#undef LV_USE_SPINBOX
#define LV_USE_SPINBOX 0
#undef LV_USE_SPINNER
#define LV_USE_SPINNER 0
#undef LV_USE_TABVIEW
#define LV_USE_TABVIEW 0
#undef LV_USE_TILEVIEW
#define LV_USE_TILEVIEW 0
#undef LV_USE_WIN
#define LV_USE_WIN 0
#undef LV_USE_SPAN
#define LV_USE_SPAN 0
#undef LV_USE_THEME_BASIC
#define LV_USE_THEME_BASIC 0
#undef LV_USE_THEME_MONO
#define LV_USE_THEME_MONO 0
#undef LV_USE_FLEX
#define LV_USE_FLEX 0
#undef LV_USE_GRID
#define LV_USE_GRID 0
#undef LV_USE_GIF
#define LV_USE_GIF 0
#undef LV_USE_MSG
#define LV_USE_MSG 0
