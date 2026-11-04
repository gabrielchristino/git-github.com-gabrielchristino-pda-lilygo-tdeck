#include "pda/bluetooth.h"
#include <assert.h>
#include <stdio.h>
static void key(uint8_t c, uint8_t mod, uint8_t code) {
    uint8_t m = 255, u = 255;
    assert(pda::hidKey(c, m, u)); assert(m == mod); assert(u == code);
}
int main() {
    for (int c = 'a'; c <= 'z'; ++c) { key(c, 0, 4 + c - 'a'); key(c - 32, 2, 4 + c - 'a'); }
    for (int c = '1'; c <= '9'; ++c) key(c, 0, 30 + c - '1');
    key('0',0,39); key(13,0,40); key(10,0,40); key(27,0,41);
    key(8,0,42); key(127,0,42); key(9,0,43); key(' ',0,44);
    key(4,1,7); key(1,1,4); key('$',2,33); key(')',2,39);
    key('=',0,46); key('+',2,46); key('\\',0,49); key('|',2,49);
    key('\x27',0,52); key('"',2,52); key('`',0,53); key('~',2,53);
    uint8_t m, u;
    for (int c = 32; c < 127; ++c) assert(pda::hidKey(c,m,u));
    assert(!pda::hidKey(0,m,u)); assert(!pda::hidKey(0xC3,m,u));
    puts("Bluetooth HID key mapping OK");
}
