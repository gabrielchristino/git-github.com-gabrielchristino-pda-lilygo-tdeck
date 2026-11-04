#pragma once
#include <stdint.h>

namespace pda {
enum class Action { None, Move, Activate, Home, Text, Backspace, Next, ToggleNavigation };
struct InputEvent {
    Action action;
    int16_t x;
    int16_t y;
    uint8_t character;
};
// Convert only WASD text events; callers decide whether text or navigation wins.
bool wasdMovement(InputEvent &event);
// Separate raw directions from the net movement: opposite pulses can cancel.
struct TrackballStats {
    uint32_t left = 0, right = 0, up = 0, down = 0;
    uint32_t polledLeft = 0, polledRight = 0, polledUp = 0, polledDown = 0;
    uint8_t levels = 0;
};

// A short click is emitted on release so a long click never opens an app first.
// Unsigned elapsed-time arithmetic also works across millis() rollover.
class CenterButton {
public:
    Action update(bool pressed, uint32_t now);
    static constexpr uint32_t debounceMs = 25;
    static constexpr uint32_t holdMs = 700;
private:
    bool raw_ = false;
    bool stable_ = false;
    bool held_ = false;
    uint32_t changedAt_ = 0;
    uint32_t pressedAt_ = 0;
};
} // namespace pda
