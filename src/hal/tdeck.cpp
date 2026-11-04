#include "tdeck.h"
#include "services.h"
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "TouchDrvGT911.hpp"

namespace pda { namespace hal {
// Original T-Deck pin map: see the LilyGO utilities.h and local reference.
static constexpr int power = 10, sda = 18, scl = 8;
static constexpr int sdCs = 39, radioCs = 9, tftCs = 12;
static constexpr int miso = 38, mosi = 41, sck = 40;
static constexpr int up = 3, down = 15, left = 1, right = 2, center = 0;
static constexpr int backlight = 42, touchInt = 16;
static constexpr int batteryAdc = 4;
static TFT_eSPI tft;
static TouchDrvGT911 touch;
static bool hasTouch = false;
static CenterButton button;
static volatile int16_t pulseX = 0, pulseY = 0;
static volatile uint32_t rawLeft = 0, rawRight = 0, rawUp = 0, rawDown = 0;
static TrackballStats polled;
static uint8_t previousLevels = 0;
static portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR pulse(volatile int16_t &axis, int step, volatile uint32_t &count) {
    portENTER_CRITICAL_ISR(&pulseMux);
    ++count;
    const int value = axis + step;
    axis = value < -32 ? -32 : (value > 32 ? 32 : value);
    portEXIT_CRITICAL_ISR(&pulseMux);
}
static void IRAM_ATTR movedUp() { pulse(pulseY, -1, rawUp); }
static void IRAM_ATTR movedDown() { pulse(pulseY, 1, rawDown); }
static void IRAM_ATTR movedLeft() { pulse(pulseX, -1, rawLeft); }
static void IRAM_ATTR movedRight() { pulse(pulseX, 1, rawRight); }
static uint8_t readBallLevels() {
    return uint8_t((digitalRead(left) ? 1 : 0) | (digitalRead(right) ? 2 : 0) |
        (digitalRead(up) ? 4 : 0) | (digitalRead(down) ? 8 : 0));
}

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, width, height);
    tft.pushColors(reinterpret_cast<uint16_t *>(pixels), width * height, false);
    tft.endWrite();
    lv_disp_flush_ready(driver);
}

static void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
    data->state = LV_INDEV_STATE_RELEASED;
    int16_t x[5], y[5];
    static bool wakeOnly = false;
    if (hasTouch && touch.isPressed() && touch.getPoint(x, y, 5) > 0) {
        if (wakeScreen()) wakeOnly = true;
        if (wakeOnly) return;
        data->point.x = x[0];
        data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else wakeOnly = false;
}

bool begin() {
    pinMode(batteryAdc, INPUT);
    analogSetPinAttenuation(batteryAdc, ADC_11db);
    pinMode(power, OUTPUT);
    digitalWrite(power, HIGH);
    delay(20);
    for (int pin : {sdCs, radioCs, tftCs}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    pinMode(miso, INPUT_PULLUP);
    SPI.begin(sck, miso, mosi);
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    pinMode(backlight, OUTPUT);
    // Reset the 16-step backlight driver, then start at its full-brightness step.
    digitalWrite(backlight, LOW);
    delay(3);
    digitalWrite(backlight, HIGH);
    delayMicroseconds(30);

    Wire.begin(sda, scl);
    Wire.setTimeOut(20);
    pinMode(touchInt, INPUT);
    touch.setPins(-1, touchInt);
    hasTouch = touch.begin(Wire, GT911_SLAVE_ADDRESS_L);
    if (hasTouch) {
        touch.setMaxCoordinates(320, 240);
        touch.setSwapXY(true);
        touch.setMirrorXY(false, true);
    }
    for (int pin : {up, down, left, right, center}) pinMode(pin, INPUT_PULLUP);
    previousLevels = readBallLevels();
    attachInterrupt(up, movedUp, CHANGE);
    attachInterrupt(down, movedDown, CHANGE);
    attachInterrupt(left, movedLeft, CHANGE);
    attachInterrupt(right, movedRight, CHANGE);

    lv_init();
    // LVGL expects a PIXEL count here, not the allocation size in bytes.
    static constexpr uint32_t pixelCount = 320 * 40;
    auto *pixels = static_cast<lv_color_t *>(ps_malloc(pixelCount * sizeof(lv_color_t)));
    if (!pixels) {
        Serial.println("PDA: falha ao alocar buffer PSRAM");
        return false;
    }
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, nullptr, pixelCount);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = 320;
    display.ver_res = 240;
    display.flush_cb = flush;
    display.draw_buf = &buffer;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t input;
    lv_indev_drv_init(&input);
    input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = readTouch;
    lv_indev_drv_register(&input);
    return true;
}

void poll(void (*dispatch)(const InputEvent &)) {
    const uint32_t now = millis();
    const uint8_t levels = readBallLevels();
    const uint8_t changes = levels ^ previousLevels;
    previousLevels = levels;
    if (changes & 1) ++polled.polledLeft;
    if (changes & 2) ++polled.polledRight;
    if (changes & 4) ++polled.polledUp;
    if (changes & 8) ++polled.polledDown;
    const Action action = button.update(digitalRead(center) == LOW, now);
    if (action != Action::None) dispatch({action, 0, 0, 0});

    static uint32_t lastBall = 0;
    if (uint32_t(now - lastBall) >= 45) {
        lastBall = now;
        portENTER_CRITICAL(&pulseMux);
        const int16_t x = pulseX, y = pulseY;
        pulseX = pulseY = 0;
        portEXIT_CRITICAL(&pulseMux);
        if (x || y) dispatch({Action::Move, x, y, 0});
    }
    static uint32_t lastKey = 0;
    if (uint32_t(now - lastKey) < 10) return;
    lastKey = now;
    Wire.requestFrom(uint8_t(0x55), uint8_t(1));
    if (!Wire.available()) return;
    const uint8_t key = Wire.read();
    if (key == 0) return;
    Action keyAction = Action::Text;
    // Factory keyboard sends Ctrl-D (0x04) for Shift + the physical $ key.
    if (key == 4) keyAction = Action::ToggleNavigation;
    else if (key == 13 || key == 10) keyAction = Action::Activate;
    else if (key == 8 || key == 127) keyAction = Action::Backspace;
    else if (key == 9) keyAction = Action::Next;
    else if (key == 27) keyAction = Action::Home;
    dispatch({keyAction, 0, 0, key});
}

bool touchAvailable() { return hasTouch; }
BatteryStats readBattery() {
    // GPIO4 measures the cell through a 2:1 divider. Average ADC noise before
    // estimating charge from 3.3-4.2 V, as in the local T-Deck reference.
    uint32_t total = 0;
    for (unsigned i = 0; i < 16; ++i) total += analogReadMilliVolts(batteryAdc);
    const int millivolts = int((total * 2 + 8) / 16);
    static BatteryStats stats;
    stats.sample(millivolts);
    return stats;
}
TrackballStats trackballStats() {
    TrackballStats result = polled;
    portENTER_CRITICAL(&pulseMux);
    result.left = rawLeft; result.right = rawRight; result.up = rawUp; result.down = rawDown;
    portEXIT_CRITICAL(&pulseMux);
    result.levels = readBallLevels();
    return result;
}
} }
