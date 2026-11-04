#pragma once
#include "pda/input.h"
#include "pda/battery.h"

namespace pda { namespace hal {
bool begin();
void poll(void (*dispatch)(const InputEvent &));
bool touchAvailable();
BatteryStats readBattery();
TrackballStats trackballStats();
} }
