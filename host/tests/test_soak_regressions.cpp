// Regression tests for the bugs the overnight soak found (docs/research/soak-audit.md,
// tools/soak/soak.cpp). Each test is the smallest reproduction of one finding.

#include "dsp/Engine.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using iv::ParamId;

struct Rig {
    iv::Engine e;
    int block;
    std::vector<float> inL, inR, outL, outR;
    uint32_t noise = 1;
    Rig(float fs, int b) : block(b), inL(size_t(b)), inR(size_t(b)), outL(size_t(b)), outR(size_t(b)) { e.prepare(fs, b); }
    // Runs `seconds` with noise at `level` (0 = silence); returns the output's mean power.
    double run(double seconds, float level)
    {
        const int blocks = int(seconds * e.sampleRate() / block);
        double acc = 0.0;
        for (int k = 0; k < blocks; ++k) {
            for (int i = 0; i < block; ++i) {
                noise = noise * 1664525u + 1013904223u;
                inL[size_t(i)] = level * float(int32_t(noise)) * (1.0f / 2147483648.0f);
                noise = noise * 1664525u + 1013904223u;
                inR[size_t(i)] = level * float(int32_t(noise)) * (1.0f / 2147483648.0f);
            }
            e.process(inL.data(), inR.data(), outL.data(), outR.data(), block);
            for (int i = 0; i < block; ++i) acc += 0.5 * (double(outL[size_t(i)]) * outL[size_t(i)] + double(outR[size_t(i)]) * outR[size_t(i)]);
        }
        return acc / double(std::max(1, blocks * block));
    }
};

// Soak finding 1: a tap that lands part-way into one of the Engine's 32-sample control
// chunks (any MIDI-note gate in the Plugin, the Renderer's sample-accurate presses) was
// read as a paint: the gesture reader's tick ran at the chunk's start, before the press,
// and `now - pressAt` wrapped round. The tap never unfroze.
void tapMidChunkUnfreezes()
{
    for (int offset : {0, 5, 31, 33, 100}) {
        Rig r(48000.0f, 128);
        r.run(1.0, 0.3f);
        r.e.button(true, 0); // freeze
        r.run(0.05, 0.0f);
        r.e.button(false, 0);
        r.run(0.5, 0.0f);
        const bool frozeOk = r.e.frozen();
        // A 50 ms tap, its press `offset` samples into a block.
        r.e.button(true, offset);
        r.run(128.0 / 48000.0, 0.0f);
        const bool paintedEarly = r.e.painting();
        r.run(0.05, 0.0f);
        r.e.button(false, 0);
        r.run(0.1, 0.0f);
        char what[160];
        std::snprintf(what, sizeof what, "a 50 ms tap pressed %d samples into a block unfreezes (no paint)", offset);
        check(frozeOk && !paintedEarly && !r.e.frozen(), what);
    }
    // The gate, as the Plugin's MIDI notes drive it.
    Rig r(44100.0f, 512);
    r.run(1.0, 0.3f);
    r.e.gate(true, 17);
    r.e.gate(false, 300);
    r.run(0.5, 0.0f);
    r.e.gate(true, 211);
    r.run(0.05, 0.0f);
    r.e.gate(false, 7);
    r.run(0.1, 0.0f);
    check(!r.e.frozen(), "a short gate (MIDI note) at odd offsets freezes, then a second one unfreezes");
}

} // namespace

int main()
{
    tapMidChunkUnfreezes();
    std::printf("%s\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
