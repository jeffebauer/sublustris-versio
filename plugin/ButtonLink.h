#pragma once
// The panel button's way to the Engine (SPEC §3 "Button + Gate"), without
// JUCE so the plugin host test can check it against a recording engine.
//
// The panel (message thread) counts presses and releases; the audio thread,
// at the start of each block, hands the Engine what happened since the last
// block: a press at the block's start, a release there too, or, when a
// whole click (press and release) fell between two blocks, the press at
// the block's start and the release at its last sample, so even a click
// shorter than a block reaches the Engine as a tap. The Engine does the
// rest (the gesture reader: tap, hold to paint; ADR 0009–0011).

#include <atomic>
#include <cstdint>

namespace iv::plugin {

struct ButtonLink {
    // Panel (message thread).
    void press() { presses_.fetch_add(1, std::memory_order_release); }
    void release() { releases_.fetch_add(1, std::memory_order_release); }

    // Audio thread, before engine.process() of a block of n samples.
    // Engine: anything with button(bool pressed, int sampleOffset).
    template <class Engine>
    void feed(Engine& engine, int n)
    {
        const uint32_t released = releases_.load(std::memory_order_acquire);
        const uint32_t pressed  = presses_.load(std::memory_order_acquire);
        const bool     down     = pressed != released; // held now
        const bool     newPress = pressed != pressesSeen_, newRelease = released != releasesSeen_;
        if (down && !downSeen_) {
            engine.button(true, 0);
        } else if (!down && downSeen_) {
            engine.button(false, 0);
        } else if (!down && newPress) { // a whole click between two blocks
            engine.button(true, 0);
            engine.button(false, n - 1);
        } else if (down && newRelease) { // let go and pressed again between two blocks
            engine.button(false, 0);
            engine.button(true, n > 1 ? 1 : 0);
        }
        pressesSeen_  = pressed;
        releasesSeen_ = released;
        downSeen_     = down;
    }

private:
    std::atomic<uint32_t> presses_{0}, releases_{0};
    uint32_t pressesSeen_ = 0, releasesSeen_ = 0; // audio thread only
    bool     downSeen_ = false;
};

} // namespace iv::plugin
