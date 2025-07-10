#pragma once
#include "hardware/TDECK_PINS.h"
#include <Arduino.h>

/**
 * Trackball driver for LilyGO T-Deck
 *
 * Usa interrupções para capturar movimentos do trackball e fornece funções utilitárias
 * para ler os deltas de movimento, cliques acumulados e detectar direções.
 *
 * Exemplo de uso:
 *
 * #include "input/trackball.h"
 * #include "utils/utils.h"
 *
 * int16_t cursor_x = 0, cursor_y = 0;
 *
 * void setup() {
 *     Serial.begin(115200);
 *     Utils::enablePeripherals();
 *     Trackball::init();
 * }
 *
 * void loop() {
 *     // --- Modo 1: Movimentação livre (ex: cursor, desenho) ---
 *     Trackball::Delta delta = Trackball::read_delta();
 *     if (delta.dx != 0 || delta.dy != 0 || delta.clicks > 0) {
 *         cursor_x += delta.dx;
 *         cursor_y += delta.dy;
 *         if (delta.clicks > 0) {
 *             Serial.println("Trackball CLICK!");
 *         }
 *         Serial.print("Cursor X: ");
 *         Serial.print(cursor_x);
 *         Serial.print(" | Y: ");
 *         Serial.println(cursor_y);
 *     }
 *
 *     // --- Modo 2: Navegação por menus (direções) ---
 *     // OBS: Não use read_delta() e funções de direção no mesmo loop!
 *     if (Trackball::moved_up())    { Serial.println("Menu: CIMA"); }
 *     if (Trackball::moved_down())  { Serial.println("Menu: BAIXO"); }
 *     if (Trackball::moved_left())  { Serial.println("Menu: ESQUERDA"); }
 *     if (Trackball::moved_right()) { Serial.println("Menu: DIREITA"); }
 *     if (Trackball::clicked())     { Serial.println("Menu: SELECIONAR"); }
 *     delay(10);
 * }
 */

namespace Trackball {

#define TRACK_SPEED 4

extern volatile bool interrupted;
extern volatile int16_t up_count;
extern volatile int16_t down_count;
extern volatile int16_t left_count;
extern volatile int16_t right_count;
extern volatile int16_t click_count;

void IRAM_ATTR ISR_up();
void IRAM_ATTR ISR_down();
void IRAM_ATTR ISR_left();
void IRAM_ATTR ISR_right();
void IRAM_ATTR ISR_click();

/**
 * Inicializa os pinos e interrupções do trackball.
 */
inline void init() {
    pinMode(TDECK_TRACKBALL_UP, INPUT_PULLUP);
    attachInterrupt(TDECK_TRACKBALL_UP, ISR_up, FALLING);
    pinMode(TDECK_TRACKBALL_DOWN, INPUT_PULLUP);
    attachInterrupt(TDECK_TRACKBALL_DOWN, ISR_down, FALLING);
    pinMode(TDECK_TRACKBALL_LEFT, INPUT_PULLUP);
    attachInterrupt(TDECK_TRACKBALL_LEFT, ISR_left, FALLING);
    pinMode(TDECK_TRACKBALL_RIGHT, INPUT_PULLUP);
    attachInterrupt(TDECK_TRACKBALL_RIGHT, ISR_right, FALLING);
    pinMode(TDECK_TRACKBALL_CLICK, INPUT_PULLUP);
    attachInterrupt(TDECK_TRACKBALL_CLICK, ISR_click, FALLING);
}

/**
 * Limpa os eventos acumulados do trackball.
 * Deve ser chamada após processar os movimentos.
 */
inline void clear_events() {
    interrupted = false;
    up_count = 1;
    down_count = 1;
    left_count = 1;
    right_count = 1;
    click_count = 0;
}

/**
 * Funções utilitárias para navegação por menus.
 * Retornam true se houve movimento na direção correspondente desde a última limpeza.
 */
inline bool moved_up() {
    if (interrupted && up_count > 1) {
        clear_events();
        return true;
    }
    return false;
}
inline bool moved_down() {
    if (interrupted && down_count > 1) {
        clear_events();
        return true;
    }
    return false;
}
inline bool moved_left() {
    if (interrupted && left_count > 1) {
        clear_events();
        return true;
    }
    return false;
}
inline bool moved_right() {
    if (interrupted && right_count > 1) {
        clear_events();
        return true;
    }
    return false;
}
inline bool clicked() {
    if (interrupted && click_count > 0) {
        clear_events();
        return true;
    }
    return false;
}

/**
 * Estrutura para resultado do trackball.
 * dx: movimento horizontal acumulado
 * dy: movimento vertical acumulado
 * clicks: quantidade de cliques detectados
 */
struct Delta {
    int16_t dx;
    int16_t dy;
    int16_t clicks;
};

/**
 * Lê e limpa os deltas acumulados do trackball.
 * Retorna um Delta com os movimentos desde a última chamada.
 */
inline Delta read_delta() {
    Delta delta = {0, 0, 0};
    if (interrupted) {
        delta.dx = -left_count + right_count;
        delta.dy = -up_count + down_count;
        delta.clicks = click_count;
        clear_events();
    }
    return delta;
}

} // namespace Trackball