#include "pda/datetime.h"
#include <stdio.h>
#include <ctype.h>
namespace pda {
bool leapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
int daysInMonth(int y, int m) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return m < 1 || m > 12 ? 0 : days[m - 1] + (m == 2 && leapYear(y));
}
bool validCivil(const CivilTime &d) {
    return d.year >= 2000 && d.year <= 2099 && d.month >= 1 && d.month <= 12 &&
        d.day >= 1 && d.day <= daysInMonth(d.year, d.month) &&
        d.hour >= 0 && d.hour < 24 && d.minute >= 0 && d.minute < 60 && d.second >= 0 && d.second < 60;
}
int64_t toUtc(const CivilTime &d, int offset) {
    if (!validCivil(d)) return 0;
    int64_t days = 0;
    for (int y = 1970; y < d.year; ++y) days += leapYear(y) ? 366 : 365;
    for (int m = 1; m < d.month; ++m) days += daysInMonth(d.year, m);
    days += d.day - 1;
    return days * 86400 + d.hour * 3600 + d.minute * 60 + d.second - offset * 60;
}
CivilTime fromUtc(int64_t epoch, int offset) {
    int64_t local = epoch + offset * 60;
    if (local < 0) local = 0;
    CivilTime result;
    int64_t days = local / 86400;
    result.hour = int(local / 3600 % 24); result.minute = int(local / 60 % 60); result.second = int(local % 60);
    result.year = 1970;
    while (result.year < 2101 && days >= (leapYear(result.year) ? 366 : 365))
        days -= leapYear(result.year++) ? 366 : 365;
    result.month = 1;
    while (result.month < 12 && days >= daysInMonth(result.year, result.month))
        days -= daysInMonth(result.year, result.month++);
    result.day = int(days) + 1;
    return result;
}
bool parseCivil(const std::string &text, CivilTime &date) {
    if (text.size() != 16 || text[2] != '/' || text[5] != '/' || text[10] != ' ' || text[13] != ':') return false;
    for (size_t i = 0; i < text.size(); ++i)
        if (i != 2 && i != 5 && i != 10 && i != 13 && !isdigit(static_cast<unsigned char>(text[i]))) return false;
    CivilTime parsed;
    if (sscanf(text.c_str(), "%d/%d/%d %d:%d", &parsed.day, &parsed.month, &parsed.year,
        &parsed.hour, &parsed.minute) != 5 || !validCivil(parsed)) return false;
    date = parsed;
    return true;
}
std::string formatCivil(int64_t epoch, int offset, bool withTime) {
    const auto d = fromUtc(epoch, offset);
    char text[40];
    if (withTime) snprintf(text, sizeof(text), "%02d/%02d/%04d %02d:%02d", d.day, d.month, d.year, d.hour, d.minute);
    else snprintf(text, sizeof(text), "%02d/%02d/%04d", d.day, d.month, d.year);
    return text;
}
uint32_t dateKey(const CivilTime &d) { return uint32_t(d.year * 10000 + d.month * 100 + d.day); }
CivilTime fromDateKey(uint32_t key) {
    CivilTime d; d.year = int(key / 10000); d.month = int(key / 100 % 100); d.day = int(key % 100);
    return validCivil(d) ? d : CivilTime{};
}
CivilTime addDays(const CivilTime &d, int days) {
    const auto candidate = fromUtc(toUtc(d, 0) + int64_t(days) * 86400, 0);
    return validCivil(candidate) ? candidate : d;
}
int weekDay(const CivilTime &d) { return int((toUtc(d, 0) / 86400 + 4) % 7); }
}
