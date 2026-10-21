#pragma once
// MIDI notes are the gate (SPEC §6), without JUCE so the plugin host test
// can check it against a recording engine. Any note, any channel, velocity
// ignored: the gate is high while at least one note is held, and each edge
// reaches the Engine at its own sample offset. The gate mirrors the button
// (ADR 0011): a rising edge is a press, a falling edge a release.

#include <array>
#include <cstddef>

namespace iv::plugin {

class MidiGate {
public:
    // Engine: anything with gate(bool high, int sampleOffset).
    template <class Engine>
    void noteOn(Engine& engine, int note, int offset)
    {
        auto& h = held_[size_t(note & 127)];
        if (h) return;
        h = true;
        if (numHeld_++ == 0) engine.gate(true, offset);
    }

    template <class Engine>
    void noteOff(Engine& engine, int note, int offset)
    {
        auto& h = held_[size_t(note & 127)];
        if (!h) return; // a note-off of a note never played does nothing
        h = false;
        if (--numHeld_ == 0) engine.gate(false, offset);
    }

    template <class Engine>
    void allOff(Engine& engine, int offset)
    {
        held_.fill(false);
        if (numHeld_ > 0) engine.gate(false, offset);
        numHeld_ = 0;
    }

    void reset()
    {
        held_.fill(false);
        numHeld_ = 0;
    }

private:
    std::array<bool, 128> held_{};
    int numHeld_ = 0;
};

} // namespace iv::plugin
