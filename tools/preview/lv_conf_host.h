#pragma once
// Use the same fonts, colors, widgets and theme as the firmware; only replace
// the Arduino tick source with LVGL's portable clock for a headless preview.
#include "../../include/lv_conf_pda.h"
#undef LV_TICK_CUSTOM
#define LV_TICK_CUSTOM 0
