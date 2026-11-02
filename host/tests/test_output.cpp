// M7: endless under the playing and the output stage (SPEC §4.9, §4.10, §7 M7; ADR 0008 amendment v1.0.61).
// Endless follows the playing: played, it sits where DECAY 0.80 does, quiet or hot, in every SPACE, without
// darkening; stopped, it holds (the duck, ADR 0014, is gone since v1.0.61);
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

// Endless follows the playing (v1.0.61, ADR 0008 amendment; the owner's pick C on renders/decay_endless_match).
// Re-pinned from the duck's checks: endless had a duck (2:1 above -24 dBFS of input, ADR 0014) that held the tail
// down while you played into a fixed floor. Now, played, endless is the longest finite tail (DECAY 0.80) exactly, so
// there's no build-up to hold back, and on its own the duck stepped the level 1.2-1.9 dB played hot: it's gone.
// So: played quiet or hot, DECAY 1 sits where 0.80 does (within 1 dB) in every SPACE; stopped, 0.80 fades and 1 holds.
struct Played { double play, after; };
Played playThenStop(float decay, int space, float amp)
{
    iv::Engine* e = makeEngine(decay);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->reset();
    const size_t n = size_t(16.0f * kSr);
    std::vector<float> in(n, 0.0f), l(n), r(n);
    gSeed = 41;
    for (size_t i = 0; i < size_t(8.0f * kSr); ++i) in[i] = amp * noise();
    for (size_t pos = 0; pos < n; pos += 48)
        e->process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
    delete e;
    auto db = [&](float a, float b) {
        double s = 0.0;
        for (size_t i = size_t(a * kSr); i < size_t(b * kSr); ++i) s += 0.5 * (double(l[i]) * l[i] + double(r[i]) * r[i]);
        return 10.0 * std::log10(s / ((b - a) * kSr) + 1e-30);
    };
    return {db(6.0f, 8.0f), db(14.0f, 16.0f)};
}

void endlessFollowsThePlaying()
{
    const char* names[3] = {"PLATE", "HALL", "MASSIVE"};
    for (int space = 0; space < 3; ++space) {
        for (float amp : {0.03f, 0.3f}) {
            const Played fin = playThenStop(0.8f, space, amp), inf = playThenStop(1.0f, space, amp);
            char msg[240];
            std::snprintf(msg, sizeof msg, "endless follows the playing, %s %s: played, DECAY 1 at %+.2f dB of 0.80 (within 1 dB); "
                                           "6 s after stopping, 0.80 has faded %.1f dB, 1 sits %+.1f dB of its playing level",
                          names[space], amp > 0.1f ? "hot" : "quiet", inf.play - fin.play, fin.play - fin.after, inf.after - inf.play);
            // Held, the tail keeps what it had when the playing stopped, less the early sound leaving (PLATE: about
            // 7 dB in the first second, the same at 0.80); 0.80 goes on fading. MASSIVE's late swell still lifts it about
            // 1 dB after the playing stops (its bloom; then flat), so it may rise up to 1.5 dB, never more.
            check(std::fabs(inf.play - fin.play) < 1.0 && inf.after - inf.play > -9.0 && inf.after > fin.after + 3.0
                      && inf.after < inf.play + 1.5, msg);
        }
    }
}

// Endless has no duck: the wet doesn't darken while you play into it.
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

void playingDoesntDarken()
{
    iv::Engine* e = makeEngine(1.0f);
    const size_t n = size_t(12.0f * kSr);
    std::vector<float> in(n, 0.0f), l(n), r(n);
    for (size_t i = 0; i < size_t(8.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (size_t pos = 0; pos < n; pos += 48)
        e->process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
    delete e;
    const double hiPlaying = highShare(l, size_t(6.0f * kSr), size_t(8.0f * kSr));
    const double hiAfter = highShare(l, size_t(9.5f * kSr), size_t(11.5f * kSr));
    char msg[220];
    std::snprintf(msg, sizeof msg, "playing into endless doesn't darken it: the wet's highs (> 4 kHz) %.1f%% while playing, %.1f%% after",
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
    endlessFollowsThePlaying();
    playingDoesntDarken();
    limiter();
    integrationSweep();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
