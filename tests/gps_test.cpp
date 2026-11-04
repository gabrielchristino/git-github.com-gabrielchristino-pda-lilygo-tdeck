#include <Arduino.h>
#include "hal/gps.h"
#include "pda/datetime.h"
#include <cassert>
#include <iostream>
uint32_t fakeMillis = 0, fakeBaud = 0;
int fakeRxPin = -1, fakeTxPin = -1;
std::deque<char> fakeRx;
FakeUsb Serial;
static std::string nmea(const std::string &body) {
    unsigned checksum = 0;
    for (char c : body) checksum ^= static_cast<unsigned char>(c);
    char suffix[8]; snprintf(suffix, sizeof(suffix), "*%02X\r\n", checksum);
    return "$" + body + suffix;
}
static void feed(const std::string &data) {
    for (char c : data) fakeRx.push_back(c);
    pda::hal::gpsPoll();
}
static std::string report() {
    Serial.output.clear(); pda::hal::gpsReport(); return Serial.output;
}
int main() {
    using namespace pda::hal;
    gpsBegin();
    int64_t epoch = 0;
    assert(!gpsUtc(epoch));
    assert(fakeRxPin == 44 && fakeTxPin == -1 && fakeBaud == 9600);
    fakeMillis = 3001; gpsPoll();
    assert(fakeRxPin == 43 && fakeTxPin == -1 && fakeBaud == 9600);
    auto invalid = nmea("GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    invalid[invalid.size() - 4] = invalid[invalid.size() - 4] == '0' ? '1' : '0';
    feed(invalid);
    assert(report().find("LOCK=0") != std::string::npos);
    assert(report().find("BAD=1") != std::string::npos);
    feed(nmea("GPTXT,01,01,01,ANTENNA OPEN"));
    assert(report().find("LOCK=1 FRESH=1") != std::string::npos);
    assert(report().find("GPS ANTENNA OPEN") != std::string::npos);
    assert(report().find("NMEA=1 NMEA_MS=3001 SIGNAL=0") != std::string::npos);
    feed(nmea("GNRMC,,V,,,,,,,,,,N"));
    assert(report().find("GPS UTC") == std::string::npos);
    char diagnostic[512];
    gpsDescribe(diagnostic, sizeof(diagnostic));
    assert(std::string(diagnostic).find("GSV: sem dados recentes") != std::string::npos);
    feed(nmea("GPGSV,1,1,00,0"));
    feed(nmea("BDGSV,1,1,00,0"));
    assert(report().find("SKY FRESH=1 SEEN=0 SIGNAL=0 CN0_MAX=0") != std::string::npos);
    gpsDescribe(diagnostic, sizeof(diagnostic));
    assert(std::string(diagnostic).find("Sem sinal GNSS reportado") != std::string::npos);
    feed(nmea("GPGSV,2,1,05,01,40,100,35,02,30,110,,03,20,120,22,04,10,130,00,1"));
    assert(report().find("SEEN=0 SIGNAL=0") != std::string::npos); // incomplete cycle
    feed(nmea("GPGSV,2,2,05,05,50,140,41,1"));
    assert(report().find("SEEN=5 SIGNAL=3 CN0_MAX=41 PEAK=41") != std::string::npos);
    assert(report().find("SIGNAL=1 SIGNAL_MS=3001 FIX=0") != std::string::npos);
    feed(nmea("GNGSV,1,1,05,01,40,100,35,1")); // aggregate must not double count
    assert(report().find("SEEN=5 SIGNAL=3") != std::string::npos);
    auto badSky = nmea("GPGSV,1,1,01,01,40,100,99,1");
    badSky[badSky.size() - 4] = 'Z'; feed(badSky);
    assert(report().find("CN0_MAX=41 PEAK=41") != std::string::npos);
    feed(nmea("BDGSV,1,1,01,11,40,100,28,1"));
    feed(nmea("GBGSV,1,1,01,11,40,100,28,1")); // BeiDou alias must not double count
    assert(report().find("SEEN=6 SIGNAL=4") != std::string::npos);
    feed(nmea("GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    assert(report().find("FIX=1 SATS=8") != std::string::npos);
    assert(report().find("FIX=1 FIX_MS=3001") != std::string::npos);
    assert(report().find("LAT=48.1173000 LON=11.5166667") != std::string::npos);
    feed(nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,281026,,,A"));
    assert(report().find("GPS UTC 2026-10-28 12:35:19") != std::string::npos);
    assert(gpsUtc(epoch) && pda::formatCivil(epoch, -180) == "28/10/2026 09:35");
    feed(nmea("GNGGA,123520,,,,,0,00,25.5,,,,,,"));
    assert(report().find("FIX=0 SATS=0") != std::string::npos);
    assert(report().find("GPS POSITION") == std::string::npos);
    assert(report().find("EVER_FIX=1") != std::string::npos);
    assert(report().find("FIX=1 FIX_MS=3001") != std::string::npos); // first fix survives loss
    feed(nmea("GNGGA,123521,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    fakeMillis += 5001;
    assert(report().find("FRESH=0") != std::string::npos);
    assert(report().find("GPS POSITION") == std::string::npos);
    const int pin = fakeRxPin; const uint32_t baud = fakeBaud;
    gpsPoll(); assert(fakeRxPin == pin && fakeBaud == baud && fakeTxPin == -1);
    char text[256]; gpsDescribe(text, sizeof(text));
    assert(std::string(text).find("SINAL SERIAL PERDIDO") != std::string::npos);
    fakeMillis += 5000;
    assert(report().find("SKY FRESH=0 SEEN=0 SIGNAL=0 CN0_MAX=0 PEAK=41") != std::string::npos);
    feed(nmea("GPGSV,2,2,05,05,50,140,50,1")); // out of order cannot restore old cycle
    assert(report().find("SKY FRESH=0") != std::string::npos);
    assert(!gpsUtc(epoch));
    // RMC alone supplies one coherent UTC sample, including day rollover.
    feed(nmea("GNRMC,235959.50,A,4807.038,N,01131.000,E,0.0,0.0,311026,,,A"));
    assert(gpsUtc(epoch) && pda::formatCivil(epoch, 0) == "31/10/2026 23:59");
    const int64_t beforeMidnight = epoch;
    fakeMillis += 600;
    assert(gpsUtc(epoch) && epoch == beforeMidnight + 1);
    assert(pda::formatCivil(epoch, 0) == "01/11/2026 00:00");
    fakeMillis += 900;
    feed(nmea("GNGGA,000001,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    assert(!gpsUtc(epoch)); // GGA time cannot refresh an old RMC date.
    auto badClock = nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,281026,,,A");
    badClock[badClock.size() - 4] = 'Z'; feed(badClock);
    assert(!gpsUtc(epoch));
    feed(nmea("GNRMC,123519,V,4807.038,N,01131.000,E,0.0,0.0,281026,,,N"));
    assert(!gpsUtc(epoch));
    feed(nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,290224,,,A"));
    assert(gpsUtc(epoch) && pda::formatCivil(epoch, 0) == "29/02/2024 12:35");
    feed(nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,290226,,,A"));
    assert(!gpsUtc(epoch)); // Invalid leap day.
    feed(nmea("GNRMC,250000,A,4807.038,N,01131.000,E,0.0,0.0,281026,,,A"));
    assert(!gpsUtc(epoch));
    feed(nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,,,,A"));
    assert(!gpsUtc(epoch)); // Missing date must not reuse the previous one.
    feed(nmea("GNRMC,,A,4807.038,N,01131.000,E,0.0,0.0,281026,,,A"));
    assert(!gpsUtc(epoch));
    feed(nmea("GNRMC,123519,A,4807.038,N,01131.000,E,0.0,0.0,281026,,,A"));
    assert(gpsUtc(epoch));
    feed(nmea("GNRMC,123520,V,,,,,,,281026,,,N"));
    assert(!gpsUtc(epoch)); // Loss of validity revokes the sample immediately.
    // A backlog larger than the old 2 KB budget is drained in one poll.
    std::string burst;
    for (unsigned i = 0; i < 100; ++i) burst += nmea("GPGSV,1,1,01,01,40,100,35,1");
    assert(burst.size() > 2048 && burst.size() < 8192);
    feed(burst);
    assert(fakeRx.empty());
    assert(report().find("HIGH_WATER=" + std::to_string(burst.size())) != std::string::npos);
    assert(report().find("FIX=1 FIX_MS=3001") != std::string::npos);
    gpsBegin();
    assert(fakeRxPin == 44 && fakeBaud == 9600);
    assert(report().find("NMEA=0 NMEA_MS=0 SIGNAL=0 SIGNAL_MS=0 FIX=0 FIX_MS=0") != std::string::npos);
    std::cout << "gps: receive-only scan, checksum, sky, fix loss, UTC validation, rollover, timezone and stale data OK\n";
}
