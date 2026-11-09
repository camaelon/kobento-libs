// Regression test for layout animation of components that are GONE before and after a layout
// change (animUpdate, LayoutAnimation.h).
//
// A column gives a GONE child the slot it would occupy, so the child's bounds move whenever
// its siblings switch visibility. animUpdate tweened that move like any other, forcing the
// component VISIBLE at full alpha: a component never shown was drawn sliding between two
// slots (on a device: a red "muted" icon flashing while leaving a screen). The tween also left
// it recorded as VISIBLE, so when it really appeared later it slid in from the stale slot
// instead of fading in place. androidx AnimateMeasure.paint draws nothing for GONE -> GONE.

#include "rccore/animation/LayoutAnimation.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failures; } \
    else         { std::fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

using namespace rccore;

static ComponentMeasure at(float y, int visibility) {
    ComponentMeasure m;
    m.id = 1;
    m.x = 10; m.y = y; m.w = 50; m.h = 20;
    m.visibility = visibility;
    return m;
}

int main() {
    const AnimSpecParams spec;   // 300 ms motion and fade, FADE_IN / FADE_OUT

    {   // GONE, then GONE in another slot: never drawn, no animation
        LayoutAnimStore store;
        ComponentMeasure m = at(0, VIS_GONE);
        animUpdate(store, 1, m, 0.0, true, spec);
        m = at(40, VIS_GONE);
        const bool running = animUpdate(store, 1, m, 1.0, true, spec);
        CHECK(!running, "GONE -> GONE (moved): no animation runs");
        CHECK(!m.isVisible() && m.alpha == 0.0f, "GONE -> GONE (moved): not drawn");
        CHECK(!store[1].live.isVisible(), "GONE -> GONE (moved): still recorded as GONE");
    }

    {   // GONE, moved while GONE, then VISIBLE: fades in at its own slot, no slide
        LayoutAnimStore store;
        ComponentMeasure m = at(0, VIS_GONE);
        animUpdate(store, 1, m, 0.0, true, spec);
        m = at(40, VIS_GONE);
        animUpdate(store, 1, m, 1.0, true, spec);
        m = at(80, VIS_VISIBLE);
        animUpdate(store, 1, m, 2.0, true, spec);
        m = at(80, VIS_VISIBLE);
        animUpdate(store, 1, m, 2.1, true, spec);   // a third of the way in
        CHECK(m.y == 80, "GONE -> VISIBLE: drawn at its slot, not sliding from an old one");
        CHECK(m.alpha > 0.0f && m.alpha < 1.0f, "GONE -> VISIBLE: fading in");
    }

    {   // VISIBLE -> GONE still fades out, and a GONE-slot move mid-fade doesn't cut it short
        LayoutAnimStore store;
        ComponentMeasure m = at(0, VIS_VISIBLE);
        animUpdate(store, 1, m, 0.0, true, spec);
        m = at(0, VIS_GONE);
        animUpdate(store, 1, m, 1.0, true, spec);
        m = at(0, VIS_GONE);
        animUpdate(store, 1, m, 1.1, true, spec);
        CHECK(m.isVisible() && m.alpha > 0.0f && m.alpha < 1.0f, "VISIBLE -> GONE: fading out");
        m = at(40, VIS_GONE);
        const bool running = animUpdate(store, 1, m, 1.15, true, spec);
        CHECK(running && m.alpha > 0.0f, "VISIBLE -> GONE, slot moved mid-fade: still fading");
    }

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
