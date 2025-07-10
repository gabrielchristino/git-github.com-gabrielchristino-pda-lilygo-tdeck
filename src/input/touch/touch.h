#pragma once
#include <Arduino.h>
#include "hardware/TDECK_PINS.h"
#include <Wire.h>
#include "TouchDrvGT911.hpp"

#ifndef GT911_SLAVE_ADDRESS_L
#define GT911_SLAVE_ADDRESS_L 0x5D
#endif

/**
 * Touch driver para LilyGO T-Deck
 *
 * Fornece funções para inicializar e ler eventos de toque, incluindo:
 * - Clique rápido (Tap)
 * - Clique longo (LongTap)
 * - Arrastar (Drag)
 *
 * Após um arrasto, não dispara Tap/LongTap ao soltar.
 *
 * Exemplo de uso:
 *
 * #include "input/touch.h"
 * #include "utils/utils.h"
 *
 * void setup() {
 *     Serial.begin(115200);
 *     Utils::enablePeripherals();
 *     Touch::init();
 * }
 *
 * void loop() {
 *     Touch::Event event = Touch::read();
 *     switch (event.type) {
 *         case Touch::Tap:
 *             Serial.println("Clique rápido! (abrir menu)");
 *             break;
 *         case Touch::LongTap:
 *             Serial.println("Clique longo! (abrir submenu)");
 *             break;
 *         case Touch::Drag:
 *             Serial.printf("Arrastando em (%d, %d)\n", event.x, event.y);
 *             break;
 *         default:
 *             break;
 *     }
 *     delay(10);
 * }
 */

namespace Touch {

    enum EventType {
        None,
        Tap,
        LongTap,
        Drag
    };

    struct Event {
        EventType type = None;
        int16_t x = 0;
        int16_t y = 0;
    };

    // Thresholds e parâmetros de detecção
    constexpr unsigned long LONG_TAP_THRESHOLD = 2000; // ms para clique longo
    constexpr unsigned long TOUCH_RELEASE_GRACE = 100; // ms para considerar toque liberado
    constexpr int DRAG_MIN_DIST = 2;                   // pixels para detectar arrasto

    static TouchDrvGT911 touch;
    static bool initialized = false;

    /**
     * Inicializa o driver de touch do T-Deck.
     */
    inline void init() {
        if (initialized) return;
        Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL);
        touch.setPins(-1, TDECK_TOUCH_INT);
        if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L)) {
            Serial.println("Touch init failed");
        } else {
            touch.setMaxCoordinates(320, 240);
            touch.setSwapXY(true);
            touch.setMirrorXY(false, true);
        }
        initialized = true;
    }

    /**
     * Lê o estado do touch e retorna um evento:
     * - Drag: enquanto arrastando
     * - Tap: clique rápido
     * - LongTap: clique longo
     * - None: nenhum evento novo
     *
     * Após um arrasto, não dispara Tap/LongTap ao soltar.
     */
    inline Event read() {
        static bool touch_down = false;
        static unsigned long touch_down_time = 0;
        static unsigned long last_seen_touch = 0;
        static int16_t prev_x = -1, prev_y = -1;
        static int16_t last_touch_x = 0, last_touch_y = 0;
        static bool was_dragging = false;

        Event evt;

        int16_t tx[5], ty[5];
        bool now_pressed = touch.isPressed();

        if (now_pressed) {
            uint8_t n = touch.getPoint(tx, ty, touch.getSupportTouchPoint());
            if (n > 0) {
                last_touch_x = tx[0];
                last_touch_y = ty[0];

                if (!touch_down) {
                    touch_down_time = millis();
                    touch_down = true;
                    prev_x = last_touch_x;
                    prev_y = last_touch_y;
                    was_dragging = false; // reset ao iniciar novo toque
                } else {
                    if (abs(last_touch_x - prev_x) > DRAG_MIN_DIST || abs(last_touch_y - prev_y) > DRAG_MIN_DIST) {
                        evt.type = Drag;
                        evt.x = last_touch_x;
                        evt.y = last_touch_y;
                        prev_x = last_touch_x;
                        prev_y = last_touch_y;
                        last_seen_touch = millis();
                        was_dragging = true; // marcou que houve drag
                        return evt;
                    }
                }
                last_seen_touch = millis();
            }
        } else {
            if (touch_down && (millis() - last_seen_touch > TOUCH_RELEASE_GRACE)) {
                unsigned long duration = millis() - touch_down_time;
                touch_down = false;
                prev_x = -1;
                prev_y = -1;

                evt.x = last_touch_x;
                evt.y = last_touch_y;

                if (was_dragging) {
                    was_dragging = false; // não retorna Tap/LongTap após drag
                    return evt; // evt.type = None
                }

                if (duration >= LONG_TAP_THRESHOLD) {
                    evt.type = LongTap;
                    return evt;
                } else if (duration > 30) { // ignora ruído
                    evt.type = Tap;
                    return evt;
                }
            }
        }

        evt.type = None;
        return evt;
    }
}