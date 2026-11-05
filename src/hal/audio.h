#pragma once
#include <stdint.h>
namespace pda { namespace hal {
struct AudioStats {
    bool initialized = false, playing = false;
    int error = 0;
    uint32_t requests = 0, completed = 0, bytes = 0;
};
bool audioAlert();
void audioTick();
void audioStop();
AudioStats audioStats();
} }
