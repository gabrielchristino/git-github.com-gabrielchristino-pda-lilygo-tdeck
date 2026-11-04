#include "pda/input.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
using pda::Action;
using pda::CenterButton;

int main() {
    // Contact bounce cannot launch an app; a stable short click launches once.
    CenterButton click;
    assert(click.update(true, 100) == Action::None);
    assert(click.update(false, 110) == Action::None);
    assert(click.update(true, 120) == Action::None);
    assert(click.update(true, 145) == Action::None);
    assert(click.update(false, 200) == Action::None);
    assert(click.update(true, 210) == Action::None);
    assert(click.update(false, 215) == Action::None);
    assert(click.update(false, 240) == Action::Activate);
    assert(click.update(false, 1000) == Action::None);

    // Holding returns home exactly once and release never activates an app.
    CenterButton hold;
    assert(hold.update(true, 0) == Action::None);
    assert(hold.update(true, 25) == Action::None);
    assert(hold.update(true, 724) == Action::None);
    assert(hold.update(true, 725) == Action::Home);
    assert(hold.update(true, 1500) == Action::None);
    assert(hold.update(false, 1600) == Action::None);
    assert(hold.update(false, 1625) == Action::None);
    assert(hold.update(true, 1700) == Action::None);
    assert(hold.update(true, 1725) == Action::None);
    assert(hold.update(false, 1750) == Action::None);
    assert(hold.update(false, 1775) == Action::Activate);

    // Both debounce and hold timers survive the 32-bit millis() rollover.
    CenterButton wrap;
    const uint32_t start = UINT32_MAX - 10;
    assert(wrap.update(true, start) == Action::None);
    assert(wrap.update(true, start + 25) == Action::None);
    assert(wrap.update(true, start + 724) == Action::None);
    assert(wrap.update(true, start + 725) == Action::Home);
    assert(wrap.update(false, start + 750) == Action::None);
    assert(wrap.update(false, start + 775) == Action::None);
    CenterButton stalled;
    assert(stalled.update(true, 0) == Action::None);
    assert(stalled.update(true, 25) == Action::None);
    assert(stalled.update(false, 1000) == Action::None);
    assert(stalled.update(false, 1025) == Action::Home);
    assert(stalled.update(false, 1050) == Action::None);
    puts("input: debounce, click, hold, re-arm, rollover and stalled loop OK");
}
