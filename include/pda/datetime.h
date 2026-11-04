#pragma once
#include <stdint.h>
#include <string>
namespace pda {
struct CivilTime { int year = 2000, month = 1, day = 1, hour = 0, minute = 0, second = 0; };
bool leapYear(int year);
int daysInMonth(int year, int month);
bool validCivil(const CivilTime &date);
int64_t toUtc(const CivilTime &date, int offsetMinutes);
CivilTime fromUtc(int64_t epoch, int offsetMinutes);
bool parseCivil(const std::string &text, CivilTime &date);
std::string formatCivil(int64_t epoch, int offsetMinutes, bool withTime = true);
uint32_t dateKey(const CivilTime &date);
CivilTime fromDateKey(uint32_t key);
CivilTime addDays(const CivilTime &date, int days);
int weekDay(const CivilTime &date); // Sunday = 0
}
