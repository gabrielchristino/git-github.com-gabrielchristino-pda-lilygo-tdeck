/*******************************************************************************
 * LilyGO T-Deck Keyboard Driver
 * https://github.com/Xinyuan-LilyGO/T-Deck/tree/master/examples/Keyboard_T_Deck_Master
 *
 * Fornece funções para inicializar e ler o teclado físico via I2C.
 *
 * Exemplo de uso:
 *
 * #include "input/keyboard.h"
 * #include "utils/utils.h"
 *
 * void setup() {
 *     Serial.begin(115200);
 *     Utils::enablePeripherals();
 *     Keyboard::init();
 * }
 *
 * void loop() {
 *     char key = Keyboard::get_key();
 *     if (key > 0) {
 *         Serial.print("Tecla pressionada: ");
 *         Serial.println(key);
 *     }
 *     delay(50);
 * }
 ******************************************************************************/

#pragma once
#include <Arduino.h>
#include "hardware/TDECK_PINS.h"
#include <Wire.h>

namespace Keyboard {

/**
 * Inicializa o teclado do T-Deck via I2C.
 * Deve ser chamada no setup().
 */
inline void init() {
    Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL);
    Wire.requestFrom(TDECK_KEYBOARD_ADDR, 1);
    if (Wire.read() == -1) {
        Serial.println("LILYGO Keyboard not online!");
    }
}

/**
 * Lê uma tecla pressionada do teclado.
 * @return Código da tecla pressionada, ou 0 se nenhuma tecla.
 */
inline char get_key() {
    Wire.requestFrom(TDECK_KEYBOARD_ADDR, 1);
    if (Wire.available() > 0) {
        return Wire.read();
    } else {
        return 0;
    }
}

} // namespace Keyboard