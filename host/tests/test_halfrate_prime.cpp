// The half-rate stage's first odd block (overnight review, 26 Oct 2026, arch F2). HALL and MASSIVE run
// their tank at 24 kHz in pairs of samples (dsp/HalfRate.h); a host block with an odd length leaves half
// a pair over. As built, the first odd block after a reset borrows one silent sample: when that block
// comes mid-stream (a host that sent 48s, then a 17), the wet gets a one-sample hole, a click, and is
// one sample late from then on. HalfRate::prime() (Engine::setHalfRatePrime, a prototype behind a hook,
// IV_HALFRATE_PRIME in the Renderer) starts with that sample queued instead.
//
// Asserted: an identity "tank" fed DC through HalfRate, blocks of 32 and then 17 / 31 from sample 9,600,
// shows no zero once primed; through the Engine with the prime on, HALL and MASSIVE after a switch from
// 48-sample blocks to 17 / 31 match the block-48 render undelayed (not one sample late). Printed: the
// same as built (the hole), for the review to quote.

#include "dsp/Engine.h"
#include "dsp/HalfRate.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

struct Identity {
    void process(const float* a, const float* b, float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i) {
            l[i] += a[i];
            r[i] += b[i];
        }
    }
};

// DC through HalfRate: 32-sample blocks to sample 9,600, then 17 / 31. The samples after the halfbands
// have settled (from 2,000) that are off 1 by more than 1e-3.
int dcHoles(bool primed, int& firstAt)
{
    iv::dsp::HalfRate<32> h;
    h.reset();
    if (primed) h.prime();
    Identity tank;
    std::vector<float> out;
    int pos = 0, k = 0;
    while (pos < 20000) {
        const int n = pos < 9600 ? 32 : ((k++ & 1) ? 31 : 17);
        std::vector<float> in(size_t(n), 1.0f), l(size_t(n), 0.0f), r(size_t(n), 0.0f);
        h.process(tank, in.data(), in.data(), l.data(), r.data(), n);
        out.insert(out.end(), l.begin(), l.end());
        pos += n;
    }
    int holes = 0;
    firstAt = -1;
    for (size_t i = 2000; i < out.size(); ++i)
        if (std::fabs(out[i] - 1.0f) > 1e-3f) {
            if (holes++ == 0) firstAt = int(i);
        }
    return holes;
}

float uniform(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (0.3f / 2147483648.0f);
}

// The Engine in `space`, noise for 2 s then silence, rendered in 48-sample blocks and again in 48s that
// switch to 17 / 31 at 2.02 s. Over the 0.1 s after the switch: the difference against the block-48
// render, undelayed and delayed one sample, in dB against it.
void engineAfterSwitch(int space, bool primed, double& undelayedDb, double& delayedDb)
{
    auto a = std::make_unique<iv::Engine>(), b = std::make_unique<iv::Engine>();
    for (auto* e : {a.get(), b.get()}) {
        e->prepare(48000.0f, 512);
#ifndef IV_FIXED_VOICINGS
        e->setHalfRatePrime(primed);
#endif
        e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
        e->setParam(iv::ParamId::Mix, 1.0f);
        e->setParam(iv::ParamId::Decay, 0.9f);
        e->setParam(iv::ParamId::Drift, 0.5f);
        e->reset();
    }
    const int total = 48000 * 3, sw = 96000 + 960;
    const size_t len = static_cast<size_t>(total);
    std::vector<float> in(len), oa(len), ob(len), ra(len), rb(len);
    uint32_t s = 7;
    for (int i = 0; i < total; ++i) in[size_t(i)] = i < 96000 ? uniform(s) : 0.0f;
    for (int p = 0; p < total; p += 48) a->process(in.data() + p, in.data() + p, oa.data() + p, ra.data() + p, 48);
    int p = 0, k = 0;
    while (p < total) {
        int n = p < sw ? 48 : ((k++ & 1) ? 31 : 17);
        if (p + n > total) n = total - p;
        b->process(in.data() + p, in.data() + p, ob.data() + p, rb.data() + p, n);
        p += n;
    }
    double d0 = 0, d1 = 0, ref = 0;
    for (int i = sw + 100; i < sw + 4900; ++i) {
        d0 += double(ob[size_t(i)] - oa[size_t(i)]) * (ob[size_t(i)] - oa[size_t(i)]);
        d1 += double(ob[size_t(i)] - oa[size_t(i - 1)]) * (ob[size_t(i)] - oa[size_t(i - 1)]);
        ref += double(oa[size_t(i)]) * oa[size_t(i)];
    }
    undelayedDb = 10.0 * std::log10(d0 / ref);
    delayedDb = 10.0 * std::log10(d1 / ref);
}

} // namespace

int main()
{
    int at = -1;
    const int holesBuilt = dcHoles(false, at);
    std::printf("       as built: DC through HalfRate, %d sample(s) off 1 after the switch to 17 / 31 blocks at 9600 (first at %d)\n",
                holesBuilt, at);
    const int holesPrimed = dcHoles(true, at);
    check(holesPrimed == 0, "primed: DC through HalfRate stays 1 through the switch to 17 / 31 blocks (" + std::to_string(holesPrimed)
                                + " samples off)");
    const char* names[] = {"PLATE", "HALL", "MASSIVE"};
    for (int space = 1; space <= 2; ++space) {
        double u0, d0, u1, d1;
        engineAfterSwitch(space, false, u0, d0);
        engineAfterSwitch(space, true, u1, d1);
        std::printf("       %s as built, 0.1 s after the switch to 17 / 31: against block 48 %.1f dB, against it one sample late %.1f dB\n",
                    names[space], u0, d0);
        std::printf("       %s primed: %.1f dB undelayed, %.1f dB one sample late\n", names[space], u1, d1);
        check(u1 < d1 - 10.0, std::string(names[space]) + " primed: after the switch the wet is where block 48 has it, not a sample late");
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
