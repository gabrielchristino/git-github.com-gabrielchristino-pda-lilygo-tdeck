#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
using String=std::string;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
extern uint32_t bleTestMillis;
class FakeSerial { public: template<class... Args> void printf(const char *,Args...) {} };
extern FakeSerial Serial;
inline uint32_t millis() { return bleTestMillis; }
inline void delay(uint32_t ms) { bleTestMillis+=ms; }
