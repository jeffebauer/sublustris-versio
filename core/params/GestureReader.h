#pragma once
// The one gesture reader for the button and the gate (ADR 0009, 0010, 0011;
// SPEC §3 "Button + Gate"). The Engine feeds it the combined level (button
// held or gate high) and the time; it says what to do.
//
//   Unfrozen, a press      → FREEZE at once (on the press: catch the chord).
//                            That press's release does nothing, however long.
//   Frozen, a press        → opens a kTapMaxSeconds window:
//     released inside it   → UNFREEZE on the release (a tap).
//     still held at its end→ PAINT until the release (stays frozen).
//
// Host-neutral and allocation-free: plain counters in samples.
#include "dsp/SizeOpt.h"

#include <cstdint>

namespace iv {

class GestureReader {
public:
    static constexpr float kTapMaxSeconds = 0.3f;

    enum class Event : uint8_t { None, Freeze, Unfreeze, PaintStart, PaintStop };

    void prepare(float sampleRate) { tapMax_ = uint64_t(kTapMaxSeconds * sampleRate); reset(); }
    void reset()
    {
        down_ = frozen_ = painting_ = armed_ = pressFroze_ = false;
        pressAt_ = 0;
    }

    // The combined level changed (at absolute sample `now`).
    Event edge(bool down, uint64_t now)
    {
        if (down == down_) return Event::None;
        down_ = down;
        if (down) {
            if (!frozen_) {
                frozen_ = true;
                pressFroze_ = true;
                return Event::Freeze;
            }
            armed_ = true;
            pressAt_ = now;
            return Event::None;
        }
        if (pressFroze_) {
            pressFroze_ = false;
            return Event::None;
        }
        if (painting_) {
            painting_ = false;
            return Event::PaintStop;
        }
        if (armed_) {
            armed_ = false;
            frozen_ = false;
            return Event::Unfreeze;
        }
        return Event::None;
    }

    // Time passing (call at least once per control chunk): a held press
    // turns into painting once the tap window has passed. `now` may be
    // earlier than a press made inside the same chunk (the Engine ticks at
    // the chunk's start): without the first test, now - pressAt_ wrapped
    // round and every tap that landed part-way into a chunk became a paint
    // and never unfroze (soak audit, docs/research/soak-audit.md).
    Event tick(uint64_t now)
    {
        if (armed_ && down_ && now >= pressAt_ && now - pressAt_ >= tapMax_) {
            armed_ = false;
            painting_ = true;
            return Event::PaintStart;
        }
        return Event::None;
    }

    bool frozen() const { return frozen_; }
    bool painting() const { return painting_; }

private:
    uint64_t tapMax_ = 14400, pressAt_ IV_ZI;
    bool     down_ IV_ZI, frozen_ IV_ZI, painting_ IV_ZI, armed_ IV_ZI, pressFroze_ IV_ZI;
};

} // namespace iv
