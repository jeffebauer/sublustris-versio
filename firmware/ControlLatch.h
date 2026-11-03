#pragma once
// Release firmware: the gate's latch and the SPACE switch's settle (v1.0.63, review 2 A5 / A6). Pure logic,
// tested on desktop through the Engine in host/tests/test_control_latch.cpp.
//
// The gate. The callback reads the gate once a block (1 ms), so a trigger shorter than that could fall between
// two reads and be lost (review 2: a 0.5 ms trigger rendered bit-identical to no gate). main.cpp has the chip
// latch every rising gate in hardware (the EXTI's pending bit, which no interrupt reads) and hands this the
// latch and the level each block. A pulse that came and went between two reads reaches the Engine as a press
// and its release in the same block, so it does what any short pulse does (SPEC §3 "Button + Gate"): it
// freezes, or, while frozen, unfreezes. A gate held high is seen by the level as before: held past 0.3 s while
// frozen, it paints. A short dip in a held-high gate is left alone (only a missed rise is recovered).
//
// The SPACE switch. libDaisy reads it raw each block, so a brushed or bouncing switch passed every flicker
// to the Engine, and each one started a flip. A new position reaches the Engine only once the switch has
// been off the last one for kSpaceSettleBlocks blocks in a row; a switch that's back sooner sends nothing at
// all. (A flip that has started and comes back before its fade bottoms out is the Engine's: it only dips.)

namespace ivctl {

// A position away from the last one sent for this many blocks (1 ms each) is a real flip.
constexpr int kSpaceSettleBlocks = 20;

struct GateLatch {
    bool last = false;
    // rose: a rising gate latched since the last block; high: the level now. send(level) once per edge.
    template <class Send>
    void block(bool rose, bool high, Send&& send)
    {
        const bool pulse = rose && !last && !high; // up and down again between two reads: a whole pulse
        if (pulse) send(true);                     // its press, then its release below
        if (pulse || high != last) send(high);
        last = high;
    }
};

struct SwitchSettle {
    // The position last sent, kept XOR 1 so zeroed memory reads 1: the centre, the Engine's own default SPACE, so
    // a switch already elsewhere at power-up flips there after the settle like any other move (the tank is empty).
    int stableX1 = 0, away = 0;
    // The position to send for this block's raw reading.
    int read(int pos)
    {
        if (pos == (stableX1 ^ 1) || ++away >= kSpaceSettleBlocks) {
            stableX1 = pos ^ 1;
            away     = 0;
        }
        return stableX1 ^ 1;
    }
};

} // namespace ivctl
