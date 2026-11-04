#include "pda/input.h"

namespace pda {
bool wasdMovement(InputEvent &event) {
    if (event.action != Action::Text) return false;
    switch (event.character) {
    case 'a': case 'A': event = {Action::Move, -1, 0, 0}; return true;
    case 'd': case 'D': event = {Action::Move, 1, 0, 0}; return true;
    case 'w': case 'W': event = {Action::Move, 0, -1, 0}; return true;
    case 's': case 'S': event = {Action::Move, 0, 1, 0}; return true;
    default: return false;
    }
}
Action CenterButton::update(bool pressed, uint32_t now) {
    if (pressed != raw_) {
        raw_ = pressed;
        changedAt_ = now;
    }
    if (stable_ != raw_ && uint32_t(now - changedAt_) >= debounceMs) {
        stable_ = raw_;
        if (stable_) {
            pressedAt_ = now;
            held_ = false;
        } else {
            const bool wasHeld = held_;
            held_ = false;
            if (wasHeld) return Action::None;
            // If the loop stalled across the threshold, release still cannot
            // turn a long hold into an accidental short click.
            return uint32_t(changedAt_ - pressedAt_) >= holdMs
                ? Action::Home : Action::Activate;
        }
    }
    if (stable_ && raw_ && !held_ && uint32_t(now - pressedAt_) >= holdMs) {
        held_ = true;
        return Action::Home;
    }
    return Action::None;
}
} // namespace pda
