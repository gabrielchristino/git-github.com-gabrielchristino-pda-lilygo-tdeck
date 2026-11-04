#pragma once
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <string>
using byte = uint8_t;
constexpr int INPUT_PULLUP = 2;
constexpr uint32_t SERIAL_8N1 = 0;
constexpr double TWO_PI = 6.283185307179586;
inline double radians(double v) { return v * TWO_PI / 360; }
inline double degrees(double v) { return v * 360 / TWO_PI; }
inline double sq(double v) { return v * v; }
extern uint32_t fakeMillis;
inline unsigned long millis() { return fakeMillis; }
inline void pinMode(int, int) {}
extern std::deque<char> fakeRx;
extern int fakeRxPin, fakeTxPin;
extern uint32_t fakeBaud;
class HardwareSerial {
public:
    explicit HardwareSerial(int) {}
    void end() { fakeRx.clear(); }
    void setRxBufferSize(size_t) {}
    void begin(uint32_t baud, uint32_t, int rx, int tx) {
        fakeRxPin = rx; fakeTxPin = tx; fakeBaud = baud;
    }
    int available() { return int(fakeRx.size()); }
    int read() { char c = fakeRx.front(); fakeRx.pop_front(); return c; }
};
struct FakeUsb {
    std::string output;
    explicit operator bool() const { return true; }
    int availableForWrite() const { return 4096; }
    void printf(const char *format, ...) {
        char buffer[2048]; va_list args; va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); output += buffer;
    }
};
extern FakeUsb Serial;
