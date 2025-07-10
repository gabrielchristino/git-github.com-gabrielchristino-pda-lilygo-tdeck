#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "hardware/TDECK_PINS.h"

namespace Utils {
    String formatTime(unsigned long milliseconds);
    void delayMs(unsigned long ms);
    int clamp(int value, int min, int max);

    /**
     * @brief Remove caracteres especiais de uma string para exibição em fontes ASCII.
     * @param str A string de entrada.
     * @return A string limpa.
     */
    inline String sanitizeString(String str) {
        str.replace("ã", "a"); str.replace("á", "a"); str.replace("à", "a"); str.replace("â", "a");
        str.replace("é", "e"); str.replace("ê", "e");
        str.replace("í", "i");
        str.replace("ó", "o"); str.replace("ô", "o");
        str.replace("ú", "u");
        str.replace("ç", "c");
        str.replace("Ã", "A"); str.replace("Á", "A"); str.replace("À", "A"); str.replace("Â", "A");
        str.replace("É", "E"); str.replace("Ê", "E");
        str.replace("Í", "I");
        str.replace("Ó", "O"); str.replace("Ô", "O");
        str.replace("Ú", "U");
        str.replace("Ç", "C");
        return str;
    }

    inline void enablePeripherals() {
        pinMode(TDECK_PERI_POWERON, OUTPUT);
        digitalWrite(TDECK_PERI_POWERON, HIGH);
        delay(500);
    }
}

#endif // UTILS_H