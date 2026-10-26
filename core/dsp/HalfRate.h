#pragma once
// A tank at half the rate (v1.0.35, ADR 0026; owner's pick E on renders/fit_cpu): the HALL / MASSIVE
// tank and its freeze twin run at 24 kHz, halfbands either side (dsp/Oversampler.h: flat to ~10.5 kHz,
// 85 dB down from 13.4 kHz), like the classic digital reverbs, whose tails stop around 11-12 kHz. On
// the module the tank was most of the CPU (M3 rounds 4-8: ~4,000 cycles a sample in fast memory alone),
// and at half the rate it does half the work. The dry and the attack keep the full band.
#include "dsp/Oversampler.h"
#include "dsp/SizeOpt.h"

namespace iv::dsp {

template <int kMaxBlock>
class HalfRate {
public:
    void reset()
    {
        for (auto& h : down_) h.reset();
        for (auto& h : up_) h.reset();
        phase_ = queued_ = 0;
    }
    // After reset(), a prototype behind Engine::setHalfRatePrime (overnight review, 26 Oct 2026, arch F2):
    // start with one silent sample queued, the state the first odd block would leave anyway. As built,
    // the first odd block after a reset is a sample short: mid-stream (a host that sends 48s, then a 17)
    // that is a one-sample hole in the tail, a click, and the wet one sample late from then on. Primed,
    // the tank's output is one sample (21 µs) late from the start and never starves.
    void prime()
    {
        queue_[0][0] = queue_[1][0] = 0.0f;
        queued_ = 1;
    }

    // n <= kMaxBlock samples in; the tank's output added into outL / outR. A block can end mid-pair, so
    // what comes back is queued and the very first block can be a sample short (unless primed).
    // Out of line: one copy for the tank and the twin (inlined in both, +2.6 KB of flash).
    template <class Tank>
    IV_NOINLINE void process(Tank& tank, const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        int h = 0;
        for (int i = 0; i < n; ++i) {
            if (phase_ == 0) {
                first_[0] = inL[i];
                first_[1] = inR[i];
            } else {
                halfIn_[0][h] = down_[0].down(first_[0], inL[i]);
                halfIn_[1][h] = down_[1].down(first_[1], inR[i]);
                halfOut_[0][h] = halfOut_[1][h] = 0.0f;
                ++h;
            }
            phase_ ^= 1;
        }
        if (h > 0) tank.process(halfIn_[0], halfIn_[1], halfOut_[0], halfOut_[1], h);
        for (int j = 0; j < h; ++j)
            for (int c = 0; c < 2; ++c) {
                up_[c].up(halfOut_[c][j], queue_[c][queued_ + 2 * j], queue_[c][queued_ + 2 * j + 1]);
            }
        queued_ += 2 * h;
        const int out = queued_ < n ? queued_ : n, lead = n - out;
        for (int i = lead; i < n; ++i) {
            outL[i] += queue_[0][i - lead];
            outR[i] += queue_[1][i - lead];
        }
        for (int i = out; i < queued_; ++i) {
            queue_[0][i - out] = queue_[0][i];
            queue_[1][i - out] = queue_[1][i];
        }
        queued_ -= out;
    }

private:
    Halfband down_[2], up_[2];
    float    first_[2] IV_ZI;
    float    halfIn_[2][kMaxBlock / 2 + 1] IV_ZI, halfOut_[2][kMaxBlock / 2 + 1] IV_ZI;
    float    queue_[2][kMaxBlock + 4] IV_ZI;
    int      phase_ IV_ZI, queued_ IV_ZI;
};

} // namespace iv::dsp
