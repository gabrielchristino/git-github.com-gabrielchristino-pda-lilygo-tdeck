#include "pda/bluetooth.h"
#include <string.h>
namespace pda {
bool hidKey(uint8_t c, uint8_t &modifier, uint8_t &usage) {
    modifier = usage = 0;
    if (c >= 'a' && c <= 'z') usage = 4 + c - 'a';
    else if (c >= 'A' && c <= 'Z') { usage = 4 + c - 'A'; modifier = 2; }
    else if (c >= '1' && c <= '9') usage = 30 + c - '1';
    else if (c == '0') usage = 39;
    else if (c == 13 || c == 10) usage = 40;
    else if (c == 27) usage = 41;
    else if (c == 8 || c == 127) usage = 42;
    else if (c == 9) usage = 43;
    else if (c == ' ') usage = 44;
    else if (c > 0 && c < 27) { modifier = 1; usage = 3 + c; }
    else {
        const char *plain = "-=[]\\;\x27`,./";
        const char *shift = "_+{}|:\"~<>?";
        const uint8_t usages[] = {45,46,47,48,49,51,52,53,54,55,56};
        const char *p = c ? strchr(plain, c) : nullptr;
        const char *s = c ? strchr(shift, c) : nullptr;
        if (p) usage = usages[p - plain];
        else if (s) { usage = usages[s - shift]; modifier = 2; }
        else {
            const char *digits = "!@#$%^&*()";
            const char *d = c ? strchr(digits, c) : nullptr;
            if (!d) return false;
            usage = 30 + (d - digits); modifier = 2;
        }
    }
    return usage != 0;
}
}
