#pragma once
#include <stdint.h>

namespace pda {
// Voltage telemetry only: the original board exposes no cell health/cycle counter.
struct BatteryStats {
    int millivolts = -1, minimum = -1, maximum = -1;
    int percent = -1;
    uint32_t samples = 0;
    bool valid() const { return millivolts >= 2500 && millivolts <= 5000; }
    void sample(int value) {
        millivolts = value;
        percent = -1;
        if (!valid()) return;
        percent = value <= 3300 ? 0 : value >= 4200 ? 100 : (value - 3300) * 100 / 900;
        if (minimum < 0 || value < minimum) minimum = value;
        if (maximum < 0 || value > maximum) maximum = value;
        ++samples;
    }
};
}
