#pragma once
#include "pda/datetime.h"
namespace pda {
// Read-only calendar calculations. Input uses the assistant's accent folding.
// Empty reply means that the phrase belongs to another assistant capability.
std::string calendarReply(const std::string &input, bool clockValid, CivilTime today);
}
