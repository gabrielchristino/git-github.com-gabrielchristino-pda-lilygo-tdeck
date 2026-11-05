#pragma once
#include <stdint.h>
namespace pda { namespace hal {
void memoryProbeTick();
void memoryReport();
void memoryResetWindow();
bool memoryReserve(uint32_t bytes);
void memoryRelease();
void memoryVerify();
void memoryCheck();
} }
