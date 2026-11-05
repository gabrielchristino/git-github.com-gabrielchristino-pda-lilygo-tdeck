#pragma once
#include <stddef.h>

namespace pda {
enum class AppId { Agenda, Calendar, Notes, Draw, Clock, Contacts, Calculator, Sheet, InputTest, Settings, Games, Map, Bluetooth, Chat };
struct AppInfo {
    AppId id;
    const char *name;
    const char *description;
};
extern const AppInfo apps[];
extern const size_t appCount;
} // namespace pda
