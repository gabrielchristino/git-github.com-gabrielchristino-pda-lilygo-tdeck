#include "trackball.h"

namespace Trackball {

volatile bool interrupted = false;
volatile int16_t up_count = 1;
volatile int16_t down_count = 1;
volatile int16_t left_count = 1;
volatile int16_t right_count = 1;
volatile int16_t click_count = 0;

void IRAM_ATTR ISR_up()    { interrupted = true; up_count <<= TRACK_SPEED; }
void IRAM_ATTR ISR_down()  { interrupted = true; down_count <<= TRACK_SPEED; }
void IRAM_ATTR ISR_left()  { interrupted = true; left_count <<= TRACK_SPEED; }
void IRAM_ATTR ISR_right() { interrupted = true; right_count <<= TRACK_SPEED; }
void IRAM_ATTR ISR_click() { interrupted = true; ++click_count; }

} // namespace Trackball