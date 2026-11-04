#pragma once
#include <stddef.h>
#include <stdint.h>

namespace pda { namespace hal {
// Receive only: no configuration or reset commands are sent to the module.
void gpsBegin();
// ESP32: background reader; poll is a fallback if task allocation fails.
// Accessors take a mutex and return a coherent snapshot of receiver state.
void gpsPoll();
void gpsReport();
void gpsDescribe(char *text, size_t size);
void gpsRaw(bool enabled);
bool gpsPosition(double &latitude, double &longitude, unsigned &satellites);
// UTC from a recent, checksum-validated RMC with active navigation status.
bool gpsUtc(int64_t &epoch);
} }
