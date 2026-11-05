#pragma once
#include <stdint.h>
extern uint32_t fakeMillis;
inline uint32_t millis() {return fakeMillis;}
