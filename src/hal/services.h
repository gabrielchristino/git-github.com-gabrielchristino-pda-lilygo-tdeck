#pragma once
#include "pda/system.h"
namespace pda { namespace hal {
StorageBackend &storage();
Platform &platform();
void beginStorage();
void activity();
void powerTick();
bool wakeScreen();
} }
