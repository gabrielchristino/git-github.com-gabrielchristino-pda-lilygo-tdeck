#include "gps.h"
#include "pda/datetime.h"
#include <Arduino.h>
#include <TinyGPS++.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#ifdef ARDUINO_ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

namespace pda { namespace hal {
namespace {
HardwareSerial uart(1);
TinyGPSPlus gps;
constexpr int pins[] = {44, 43};
constexpr uint32_t rates[] = {9600, 38400, 115200, 4800, 19200, 57600};
unsigned candidate = 0;
uint32_t since = 0, lastSentence = 0, received = 0;
bool locked = false, raw = false, sentenceFix = false, utcPresent = false;
char line[160] = {}, latest[160] = {};
char rmc[160] = {}, gga[160] = {}, antenna[24] = "SEM INFORMACAO";
size_t length = 0;
uint32_t started = 0;
bool everFix = false;
int64_t clockEpoch = 0;
uint32_t clockAt = 0, clockFraction = 0;
unsigned peakSignal = 0;
uint32_t firstNmeaMs = 0, firstSignalMs = 0, firstFixMs = 0;
bool signalAcquired = false;
uint32_t lastPoll = 0, maxPollGap = 0;
unsigned rxHighWater = 0;
constexpr size_t rxBufferSize = 8192;
std::atomic<uint32_t> bufferFull{0}, fifoOverflow{0}, framingErrors{0};
#ifdef ARDUINO_ARCH_ESP32
SemaphoreHandle_t gpsMutex = nullptr;
TaskHandle_t gpsTask = nullptr;
struct Guard {
    Guard() { if (gpsMutex) xSemaphoreTake(gpsMutex, portMAX_DELAY); }
    ~Guard() { if (gpsMutex) xSemaphoreGive(gpsMutex); }
};
#else
struct Guard { Guard() {} ~Guard() {} };
#endif
// Commit only complete, ordered GSV cycles. Keep talkers separate; GN is
// an aggregate fallback, never added to constellation-specific counts.
struct Sky {
    char talker[3] = {};
    unsigned next = 0, pages = 0, pendingSeen = 0, pendingSignal = 0, pendingMax = 0;
    unsigned seen = 0, signal = 0, strongest = 0;
    uint32_t updated = 0, cycleAt = 0;
    bool valid = false;
} sky[8];
bool skyFresh(const Sky &s) { return s.valid && uint32_t(millis() - s.updated) < 10000; }
int rx() { return pins[candidate % 2]; }
uint32_t baud() { return rates[candidate / 2]; }
bool fresh() { return locked && uint32_t(millis() - lastSentence) < 5000; }
bool fix() { return fresh() && sentenceFix && gps.location.isValid() && gps.location.age() < 5000; }
const char *state() {
    if (fix()) return "POSICAO OK";
    if (fresh()) return "NMEA OK / SEM POSICAO";
    return locked ? "SINAL SERIAL PERDIDO" : "PROCURANDO NMEA";
}
const char *field(unsigned index) {
    const char *p = line;
    while (index-- && p) { p = strchr(p, ','); if (p) ++p; }
    return p ? p : "";
}
int number(unsigned index) {
    const char *p = field(index);
    if (*p < '0' || *p > '9') return -1;
    int value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p++ - '0');
        if (value > 999) return -1;
    }
    return *p == ',' || *p == '*' ? value : -1;
}
void readClock() {
    clockEpoch = 0;
    // Require date and time in this same RMC; TinyGPS retains old fields.
    const char *time = field(1), *date = field(9);
    if (*field(2) != 'A' || strcspn(time, ",*") < 6 || strcspn(date, ",*") != 6) return;
    for (unsigned i = 0; i < 6; ++i)
        if (time[i] < '0' || time[i] > '9' || date[i] < '0' || date[i] > '9') return;
    if (!gps.date.isValid() || !gps.time.isValid()) return;
    CivilTime utc;
    utc.year = gps.date.year(); utc.month = gps.date.month(); utc.day = gps.date.day();
    utc.hour = gps.time.hour(); utc.minute = gps.time.minute(); utc.second = gps.time.second();
    if (!validCivil(utc) || utc.year < 2020) return;
    clockEpoch = toUtc(utc, 0);
    clockAt = millis(); clockFraction = gps.time.centisecond() * 10;
}
void readSky() {
    const int pages = number(1), page = number(2), seen = number(3);
    if (pages < 1 || pages > 32 || page < 1 || page > pages || seen < 0) return;
    Sky *entry = nullptr;
    for (auto &s : sky) if (!strncmp(s.talker, line + 1, 2)) { entry = &s; break; }
    if (!entry) for (auto &s : sky) if (!s.talker[0]) {
        entry = &s; memcpy(s.talker, line + 1, 2); break;
    }
    if (!entry) return;
    auto &s = *entry;
    if (page == 1) {
        s.next = 1; s.pages = unsigned(pages); s.pendingSeen = unsigned(seen);
        s.pendingSignal = s.pendingMax = 0; s.cycleAt = millis();
    }
    if (unsigned(page) != s.next || unsigned(pages) != s.pages ||
        unsigned(seen) != s.pendingSeen || uint32_t(millis() - s.cycleAt) >= 10000) {
        s.next = 0; return;
    }
    for (unsigned i = 0; i < 4; ++i) {
        const int id = number(4 + i * 4), cn = number(7 + i * 4);
        if (id > 0 && cn > 0 && cn <= 99) {
            ++s.pendingSignal;
            if (unsigned(cn) > s.pendingMax) s.pendingMax = unsigned(cn);
        }
    }
    ++s.next;
    if (page == pages) {
        s.seen = s.pendingSeen; s.signal = s.pendingSignal; s.strongest = s.pendingMax;
        s.updated = millis(); s.valid = true; s.next = 0;
        if (s.strongest > peakSignal) peakSignal = s.strongest;
        if (s.signal && !signalAcquired) {
            signalAcquired = true; firstSignalMs = millis() - started;
        }
    }
}
void skySummary(unsigned &seen, unsigned &signal, unsigned &strongest, bool &available) {
    seen = signal = strongest = 0; available = false;
    bool specific = false;
    for (const auto &s : sky) if (skyFresh(s) && strcmp(s.talker, "GN")) specific = true;
    for (const auto &s : sky) if (skyFresh(s) && (!specific || strcmp(s.talker, "GN"))) {
        // GB and BD are aliases for BeiDou; prefer GB if both are present.
        bool alias = false;
        if (!strcmp(s.talker, "BD")) for (const auto &other : sky)
            if (!strcmp(other.talker, "GB") && skyFresh(other)) alias = true;
        if (alias) continue;
        available = true; seen += s.seen; signal += s.signal;
        if (s.strongest > strongest) strongest = s.strongest;
    }
}
void startCandidate() {
    uart.end();
    pinMode(43, INPUT_PULLUP);
    pinMode(44, INPUT_PULLUP);
    uart.setRxBufferSize(rxBufferSize);
    // Explicit RX and no TX; neither Grove signal is ever driven by this service.
    uart.begin(baud(), SERIAL_8N1, rx(), -1);
#ifdef ARDUINO_ARCH_ESP32
    // end() clears callbacks, so reinstall after every scan candidate.
    uart.onReceiveError([](hardwareSerial_error_t error) {
        if (error == UART_BUFFER_FULL_ERROR) ++bufferFull;
        else if (error == UART_FIFO_OVF_ERROR) ++fifoOverflow;
        else if (error == UART_FRAME_ERROR || error == UART_PARITY_ERROR) ++framingErrors;
    });
#endif
    gps = TinyGPSPlus();
    length = 0;
    sentenceFix = false;
    utcPresent = false;
    clockEpoch = 0;
    rmc[0] = gga[0] = 0;
    snprintf(antenna, sizeof(antenna), "SEM INFORMACAO");
    latest[0] = 0;
    for (auto &s : sky) s = Sky();
    since = millis();
}
}
namespace {
void pollReceiver() {
    Guard guard;
    const uint32_t now = millis();
    const uint32_t gap = now - lastPoll;
    if (gap > maxPollGap) maxPollGap = gap;
    lastPoll = now;
    const unsigned queued = unsigned(uart.available());
    if (queued > rxHighWater) rxHighWater = queued;
    int budget = rxBufferSize;
    while (uart.available() && budget-- > 0) {
        const char c = char(uart.read());
        ++received;
        if (c == '$') length = 0;
        if (c >= 32 && c <= 126 && length < sizeof(line) - 1) {
            line[length++] = c;
            line[length] = 0;
        }
        if (gps.encode(c)) {
            lastSentence = millis();
            if (!locked) {
                locked = true;
                firstNmeaMs = millis() - started;
                Serial.printf("GPS LOCK RX=%d BAUD=%lu\n", rx(), (unsigned long)baud());
            }
            // TinyGPS retains the last position after a no-fix sentence. Track
            // the current validity too, so stale coordinates never imply a fix.
            if (length >= 7 && line[0] == '$') {
                if (!strncmp(line + 3, "GGA,", 4)) {
                    sentenceFix = *field(6) >= '1' && *field(6) <= '5';
                    snprintf(gga, sizeof(gga), "%s", line);
                } else if (!strncmp(line + 3, "RMC,", 4)) {
                    sentenceFix = *field(2) == 'A';
                    utcPresent = *field(1) >= '0' && *field(1) <= '9' && *field(9) >= '0' && *field(9) <= '9';
                    snprintf(rmc, sizeof(rmc), "%s", line);
                    readClock();
                }
                if (!strncmp(line + 3, "TXT,", 4)) {
                    if (strstr(line, "ANTENNA OPEN")) snprintf(antenna, sizeof(antenna), "OPEN");
                    else if (strstr(line, "ANTENNA SHORT")) snprintf(antenna, sizeof(antenna), "SHORT");
                    else if (strstr(line, "ANTENNA OK")) snprintf(antenna, sizeof(antenna), "OK");
                }
                if (!strncmp(line + 3, "GSV,", 4)) readSky();
            }
            if (fix() && !everFix) { everFix = true; firstFixMs = millis() - started; }
            snprintf(latest, sizeof(latest), "%s", line);
            // Explicit raw monitoring; discard samples if the host cannot keep
            // up rather than blocking the PDA's event loop.
            if (raw && Serial &&
                Serial.availableForWrite() >= int(strlen(latest) + 12)) {
                Serial.printf("GPS NMEA %s\n", latest);
            }
        }
    }
    if (!locked && uint32_t(millis() - since) >= 3000) {
        candidate = (candidate + 1) % (2 * (sizeof(rates) / sizeof(rates[0])));
        startCandidate();
    }
    // Keep the detected setting while reception is interrupted. Rescanning a
    // working module would discard position and hide an intermittent wire.
}
#ifdef ARDUINO_ARCH_ESP32
void receiveTask(void *) {
    for (;;) { pollReceiver(); vTaskDelay(pdMS_TO_TICKS(10)); }
}
#endif
}
void gpsBegin() {
#ifdef ARDUINO_ARCH_ESP32
    if (gpsTask) return;
    if (!gpsMutex) gpsMutex = xSemaphoreCreateMutex();
#endif
    started = lastPoll = millis(); everFix = signalAcquired = false; peakSignal = 0;
    firstNmeaMs = firstSignalMs = firstFixMs = maxPollGap = 0;
    rxHighWater = 0; bufferFull = 0; fifoOverflow = 0; framingErrors = 0;
    candidate = 0; locked = false; received = lastSentence = 0;
    startCandidate();
#ifdef ARDUINO_ARCH_ESP32
    if (gpsMutex && xTaskCreate(receiveTask, "gps_rx", 4096, nullptr, 2, &gpsTask) != pdPASS)
        gpsTask = nullptr;
    Serial.printf("GPS READER %s BUFFER=%u\n", gpsTask ? "TASK" : "LOOP", unsigned(rxBufferSize));
#endif
}
void gpsPoll() {
#ifdef ARDUINO_ARCH_ESP32
    if (gpsTask) return;
#endif
    pollReceiver();
}
void gpsReport() {
    Guard guard;
    const bool positioned = fix();
    Serial.printf("GPS RX=%d BAUD=%lu LOCK=%d FRESH=%d BYTES=%lu VALID=%lu BAD=%lu FIX=%d SATS=%lu HDOP=%.2f AGE_MS=%lu STATE=%s\n",
        rx(), (unsigned long)baud(), locked, fresh(), (unsigned long)received,
        (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum(), positioned,
        (unsigned long)(gps.satellites.isValid() && gps.satellites.age() < 5000 ? gps.satellites.value() : 0),
        gps.hdop.isValid() && gps.hdop.age() < 5000 ? gps.hdop.hdop() : -1.0,
        (unsigned long)gps.location.age(), state());
    if (positioned) Serial.printf("GPS POSITION LAT=%.7f LON=%.7f ALT_M=%.1f SPEED_KMH=%.2f\n",
        gps.location.lat(), gps.location.lng(), gps.altitude.meters(), gps.speed.kmph());
    if (fresh() && utcPresent && gps.date.isValid() && gps.time.isValid() && gps.time.age() < 5000 &&
        gps.date.age() < 5000 && gps.date.year() >= 2020 && gps.date.year() <= 2100 &&
        gps.date.month() >= 1 && gps.date.month() <= 12 && gps.date.day() >= 1 && gps.date.day() <= 31 &&
        gps.time.hour() < 24 && gps.time.minute() < 60 && gps.time.second() < 60)
        Serial.printf("GPS UTC %04u-%02u-%02u %02u:%02u:%02u\n", gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    Serial.printf("GPS ANTENNA %s\n", antenna);
    unsigned seen, signal, strongest; bool available;
    skySummary(seen, signal, strongest, available);
    Serial.printf("GPS SKY FRESH=%d SEEN=%u SIGNAL=%u CN0_MAX=%u PEAK=%u EVER_FIX=%d ELAPSED_S=%lu\n",
        available, seen, signal, strongest, peakSignal, everFix, (unsigned long)((millis() - started) / 1000));
    if (rmc[0]) Serial.printf("GPS RMC %s\n", rmc);
    if (gga[0]) Serial.printf("GPS GGA %s\n", gga);
    if (latest[0]) Serial.printf("GPS LAST %s\n", latest);
    Serial.printf("GPS ACQUISITION NMEA=%d NMEA_MS=%lu SIGNAL=%d SIGNAL_MS=%lu FIX=%d FIX_MS=%lu\n",
        locked, (unsigned long)firstNmeaMs, signalAcquired, (unsigned long)firstSignalMs,
        everFix, (unsigned long)firstFixMs);
    Serial.printf("GPS UART BUFFER=%u HIGH_WATER=%u FULL=%lu OVERFLOW=%lu FRAME=%lu MAX_POLL_GAP_MS=%lu\n",
        unsigned(rxBufferSize), rxHighWater, (unsigned long)bufferFull.load(),
        (unsigned long)fifoOverflow.load(), (unsigned long)framingErrors.load(), (unsigned long)maxPollGap);
}
void gpsDescribe(char *text, size_t size) {
    Guard guard;
    const unsigned long sats = gps.satellites.isValid() && gps.satellites.age() < 5000 ? gps.satellites.value() : 0;
    unsigned seen, signal, strongest; bool available;
    skySummary(seen, signal, strongest, available);
    char reception[80], position[64], age[24], acquisition[32];
    if (everFix) snprintf(acquisition, sizeof(acquisition), "1o fix: %lus", (unsigned long)(firstFixMs / 1000));
    else snprintf(acquisition, sizeof(acquisition), "fix: aguardando");
    if (available) snprintf(reception, sizeof(reception), "Reportados: %u / com sinal: %u\nC/N0 max: %u dB-Hz / pico: %u", seen, signal, strongest, peakSignal);
    else snprintf(reception, sizeof(reception), "GSV: sem dados recentes\nPico C/N0: %u dB-Hz (sessao)", peakSignal);
    if (locked) snprintf(age, sizeof(age), "%lus", (unsigned long)((millis() - lastSentence) / 1000));
    else snprintf(age, sizeof(age), "--");
    if (fix()) snprintf(position, sizeof(position), "%.5f, %.5f", gps.location.lat(), gps.location.lng());
    else snprintf(position, sizeof(position), "%s", !fresh() ? "Confira alimentacao e fios" :
        !available ? "Aguardando mensagens GSV" : !signal ? "Sem sinal GNSS reportado" : "Recebendo; aguardando posicao");
    const unsigned long seconds = (millis() - started) / 1000;
    snprintf(text, size, "%s\nLigado: %lu:%02lu / %s\nAntena: %s / usados: %lu\n%s\nNMEA: %lu / erros: %lu / perdas: %lu\nUltimo NMEA: %s / RX%d\n%s",
        fix() ? "GPS: POSICAO OK" : fresh() ? "GPS: SERIAL OK / SEM POSICAO" : state(),
        seconds / 60, seconds % 60, acquisition, antenna, sats, reception,
        (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum(),
        (unsigned long)(bufferFull.load() + fifoOverflow.load()), age, rx(), position);
}
void gpsRaw(bool enabled) { Guard guard; raw = enabled; }
bool gpsUtc(int64_t &epoch) {
    Guard guard;
    const uint32_t age = uint32_t(millis() - clockAt);
    if (!fresh() || !clockEpoch || age >= 1500) return false;
    epoch = clockEpoch + (age + clockFraction) / 1000;
    return true;
}
bool gpsPosition(double &latitude, double &longitude, unsigned &satellites) {
    Guard guard;
    satellites = gps.satellites.isValid() && gps.satellites.age()<5000 ? gps.satellites.value() : 0;
    if(!fix()) { latitude=longitude=0; return false; }
    latitude=gps.location.lat(); longitude=gps.location.lng(); return true;
}
} }
