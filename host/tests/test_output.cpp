// M7: ducking and the output stage (SPEC §4.9, §4.10, §7 M7; ADR 0014).
// Ducking (4:1 compression of the wet since v1.0.17) only from DECAY 0.85 up,
// deepening to full at 1; the wet steps back while you play, without
// darkening, and comes back after; no duck while frozen;
// the soft limiter keeps the wet under −1 dBFS and reports its gain; MIX 0
// stays a clean passthrough. Then the integration sweep: random settings of
// every control with freezes, paints and SPACE / ATTACK flips: finite,
// bounded, click-free.

#include "Metrics.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;
constexpr float kSr = 48000.0f;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

uint32_t gSeed = 41;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}
float uniform01() { return 0.5f + 0.5f * noise(); }

iv::Engine* makeEngine(float decay, float mix = 1.0f)
{
    auto* e = new iv::Engine;
    e->prepare(kSr, 48);
    e->setParam(iv::ParamId::Decay, decay);
    e->setParam(iv::ParamId::Mix, mix);
    e->setParam(iv::ParamId::Era, 0.0f);
    e->setParam(iv::ParamId::Time, 0.0f);
    e->reset();
    return e;
}

// Max duck amount while a loud input plays.
float duckWhilePlaying(float decay, bool frozen)
{
    iv::Engine* e = makeEngine(decay);
    std::vector<float> in(48), l(48), r(48);
    float mx = 0.0f;
    for (int b = 0; b < 3000; ++b) {
        for (auto& x : in) x = 0.3f * noise();
        if (frozen && b == 10) e->button(true, 0);
        if (frozen && b == 12) e->button(false, 0);
        e->process(in.data(), in.data(), l.data(), r.data(), 48);
        if (b > 400) mx = std::fmax(mx, e->duckAmount()); // after the freeze's 0.2 s glide (a freeze lets the duck go in 50 ms)
    }
    delete e;
    return mx;
}

void duckDepth()
{
    const float d80 = duckWhilePlaying(0.8f, false), d90 = duckWhilePlaying(0.9f, false), d100 = duckWhilePlaying(1.0f, false);
    char msg[200];
    std::snprintf(msg, sizeof msg, "ducking depth: %.2f at DECAY 0.80, %.2f at 0.90, %.2f at 1.0 (0 below 0.85, rising)",
                  d80, d90, d100);
    check(d80 == 0.0f && d90 > 0.0f && d100 >= d90 && d100 > 0.5f, msg);
    std::snprintf(msg, sizeof msg, "no duck while frozen (playing over a pad mustn't duck it): %.2f", duckWhilePlaying(1.0f, true));
    check(duckWhilePlaying(1.0f, true) == 0.0f, msg);
}

// While playing at infinite the wet steps back (4:1), with no darkening, and
// it lets go within about a second of stopping (release 0.4 s).
double highShare(const std::vector<float>& x, size_t a, size_t b)
{
    const float c = std::exp(-2.0f * 3.14159265f * 4000.0f / kSr);
    float hx = 0.0f, hy = 0.0f;
    double hi = 0.0, all = 0.0;
    for (size_t i = a; i < b; ++i) {
        hy = c * (hy + x[i] - hx);
        hx = x[i];
        hi += double(hy) * hy;
        all += double(x[i]) * x[i];
    }
    return hi / (all + 1e-30);
}

void ducksAndComesBack()
{
    iv::Engine* e = makeEngine(1.0f);
    const size_t n = size_t(12.0f * kSr);
    std::vector<float> in(n, 0.0f), l(n), r(n);
    for (size_t i = 0; i < size_t(8.0f * kSr); ++i) in[i] = 0.3f * noise();
    float during = 0.0f, after = 1.0f;
    for (size_t pos = 0; pos < n; pos += 48) {
        e->process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        if (pos > size_t(6.0f * kSr) && pos < size_t(8.0f * kSr)) during = std::fmax(during, e->duckAmount());
        if (pos > size_t(9.5f * kSr)) after = std::fmin(after, e->duckAmount());
    }
    delete e;
    const double hiPlaying = highShare(l, size_t(6.0f * kSr), size_t(8.0f * kSr));
    const double hiAfter = highShare(l, size_t(9.5f * kSr), size_t(11.5f * kSr));
    char msg[220];
    std::snprintf(msg, sizeof msg, "ducking: %.2f while playing, %.2f 1.5 s after stopping (lets go)", during, after);
    check(during > 0.5f && after < 0.05f, msg);
    std::snprintf(msg, sizeof msg, "ducking doesn't darken: the wet's highs (> 4 kHz) %.1f%% while playing, %.1f%% after",
                  100.0 * hiPlaying, 100.0 * hiAfter);
    check(hiPlaying > 0.5 * hiAfter, msg);
}

// Driven hot: a full-scale input into a long, bright PLATE. ERA 0, the Neve alone: the wear's tape
// (v1.0.42) soft-limits the wet's peaks itself, so the limiter would have less to do. At infinite the floor
// governor would hold the tank down, so this is a finite DECAY.
void limiter()
{
    iv::Engine* e = makeEngine(0.6f);
    e->setParam(iv::ParamId::Space, 0.0f);
    e->setParam(iv::ParamId::Size, 1.0f);
    e->setParam(iv::ParamId::Era, 0.0f);
    e->setParam(iv::ParamId::Tone, 1.0f);
    e->reset();
    const size_t n = size_t(10.0f * kSr);
    std::vector<float> in(n), l(n), r(n);
    for (size_t i = 0; i < n; ++i) in[i] = noise() > 0.0f ? 0.99f : -0.99f;
    float minGain = 1.0f, peak = 0.0f;
    for (size_t pos = 0; pos < n; pos += 48) {
        e->process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        minGain = std::fmin(minGain, e->limiterGain());
    }
    for (size_t i = 0; i < n; ++i) peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
    delete e;
    char msg[200];
    std::snprintf(msg, sizeof msg, "limiter: a hot wet peaks at %.5f (ceiling %.5f) and reports its gain (%.2f)", peak,
                  iv::voicing::kLimitCeiling, minGain);
    check(peak <= iv::voicing::kLimitCeiling + 1e-4f && minGain < 0.99f, msg);

    iv::Engine* dry = makeEngine(1.0f, 0.0f);
    std::vector<float> dl(n), dr(n);
    for (size_t pos = 0; pos < n; pos += 48) dry->process(in.data() + pos, in.data() + pos, dl.data() + pos, dr.data() + pos, 48);
    delete dry;
    bool same = true;
    for (size_t i = 0; i < n; ++i) same &= dl[i] == in[i];
    check(same, "MIX 0 is still a bit-identical passthrough at infinite DECAY with a hot input");
}

// SPEC §7 M7: every control at random settings, with freezes, paints and
// SPACE / ATTACK flips, on a tone with noise: finite, bounded, no clicks.
void integrationSweep()
{
    const size_t n = size_t(6.0f * kSr);
    std::vector<float> in(n);
    for (size_t i = 0; i < n; ++i)
        in[i] = 0.25f * std::sin(2.0f * 3.14159265f * 196.0f * float(i) / kSr) + 0.03f * noise();
    int bad = 0, clicks = 0;
    const int runs = 40;
    for (int run = 0; run < runs; ++run) {
        auto* e = new iv::Engine;
        e->prepare(kSr, 48);
        for (const auto& p : iv::kParams) e->setParam(p.id, uniform01());
        e->reset();
        std::vector<float> l(n), r(n);
        for (size_t pos = 0; pos < n; pos += 48) {
            const double t = double(pos) / kSr;
            if (pos % size_t(kSr) == 0 && pos > 0) { // once a second, move something
                const auto& p = iv::kParams[size_t(uniform01() * float(iv::kParams.size())) % iv::kParams.size()];
                e->setParam(p.id, p.kind == iv::ParamKind::Switch3 ? iv::switchToNormalised(int(uniform01() * 3.0f) % 3)
                                                                   : uniform01());
            }
            if (std::fabs(t - 1.5) < 0.0005) e->button(true, 0);
            if (std::fabs(t - 1.6) < 0.0005) e->button(false, 0);
            if (std::fabs(t - 3.0) < 0.0005) e->gate(true, 0);
            if (std::fabs(t - 4.0) < 0.0005) e->gate(false, 0);
            e->process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        }
        float pk = 0.0f;
        bool fin = true;
        for (size_t i = 0; i < n; ++i) {
            fin &= std::isfinite(l[i]) && std::isfinite(r[i]);
            pk = std::fmax(pk, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
        }
        const auto m = iv::metrics::compute({l, r}, kSr);
        bad += !fin || pk > 1.2f;
        clicks += m.clickCount > 0;
        delete e;
    }
    check(bad == 0, "integration: " + std::to_string(runs) + " random settings with moves, freezes, paints: all finite and bounded");
    check(clicks == 0, "integration: no click-detector hits (" + std::to_string(clicks) + " of " + std::to_string(runs) + " runs)");
}

} // namespace

int main()
{
    duckDepth();
    ducksAndComesBack();
    limiter();
    integrationSweep();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
