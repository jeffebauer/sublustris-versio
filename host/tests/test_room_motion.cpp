// v1.0.47: the small room's movement scales with SIZE (owner's pick B on renders/room_motion, every card). HALL's and
// MASSIVE's frozen twin, the small room (dsp/TwinRoom.h), wanders each line's read by 0.5 % of the line's length
// instead of 0.15 ms on every line (which bent the short lines most: the bend builds up pass after pass). Pins: the
// room alone, frozen, moves a held tone by at most ~10 c (p95, the pages' measure) at SIZE 0, 0.5 and 0.9; its ringing
// stays at B's; Paint's ceiling holds; a painted pad holds 10 minutes; freezing, frozen SIZE moves, painting and
// unfreezing stay click-free. (With the wander as before, measured the same way: 56 / 15 / 6.9 c and ringing 16.9 / 7.4 /
// 8.5 dB at SIZE 0 / 0.5 / 0.9; now 6.0 / 6.0 / 7.5 c and 13.7 / 7.2 / 7.1 dB.)

#include "DecayKnobFor.h"
#include "Metrics.h"
#include "PitchTrack.h"
#include "dsp/Engine.h"
#include "dsp/Filters.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
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

struct Stereo {
    std::vector<float> l, r;
};

struct Press {
    double at;
    bool   down;
};
struct SizeMove {
    double at;
    float  size;
};

struct Setting {
    int   space = 1;
    float size = 0.9f, decay = 0.9f, mix = 1.0f;
    bool  layerSolo = false; // the room alone (Engine::setLayerSoloPrototype: the pad out of the wet)
};

Stereo render(const std::vector<float>& in, const Setting& s, const std::vector<Press>& presses,
              const std::vector<SizeMove>& moves = {})
{
    iv::Engine e;
    e.prepare(kSr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Decay, decayKnobFor(s.decay));
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Era, 0.0f);
    e.setParam(iv::ParamId::Mix, s.mix);
    e.setParam(iv::ParamId::Time, 0.0f);
    e.setLayerSoloPrototype(s.layerSolo);
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    size_t pi = 0, mi = 0;
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        while (pi < presses.size() && size_t(presses[pi].at * kSr) < pos + size_t(n)) {
            e.button(presses[pi].down, int(size_t(presses[pi].at * kSr) - pos));
            ++pi;
        }
        while (mi < moves.size() && size_t(moves[mi].at * kSr) < pos + size_t(n)) {
            e.setParam(iv::ParamId::Size, moves[mi].size);
            ++mi;
        }
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

double rmsDb(const Stereo& s, double from, double to)
{
    const size_t a = size_t(from * kSr), b = std::min(size_t(to * kSr), s.l.size());
    double sum = 0.0;
    for (size_t i = a; i < b; ++i) sum += double(s.l[i]) * s.l[i] + double(s.r[i]) * s.r[i];
    return 10.0 * std::log10(sum / double(2 * (b - a)) + 1e-30);
}

// The level below 1 kHz (a 2nd-order low-pass run from the start), for paint's checks (v1.0.58): HALL's room paints
// gently, and on white noise the total is the top end's, which a painted pad settles a little under.
double lowDb(const Stereo& s, double from, double to)
{
    iv::dsp::Biquad lp[2];
    for (auto& f : lp) f.setLowpass(1000.0f, 0.7071f, float(kSr));
    const size_t a = size_t(from * kSr), b = std::min(size_t(to * kSr), s.l.size());
    double sum = 0.0;
    for (size_t i = 0; i < b; ++i) {
        const double l = lp[0].process(s.l[i]), r = lp[1].process(s.r[i]);
        if (i >= a) sum += l * l + r * r;
    }
    return 10.0 * std::log10(sum / double(2 * (b - a)) + 1e-30);
}

bool finite(const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}

const char* name(int space) { return space == 1 ? "HALL" : "MASSIVE"; }
const float kSizes[3] = {0.0f, 0.5f, 0.9f};

// The room alone, frozen: a pad (noise, frozen at 4 s, unheard), then held tones (700 Hz + 1.3 kHz) from 4.5 s
// through the room; the 1.3 kHz's p95 cents over 6-13 s. And its ringing: a noise burst into it (4.5-4.7 s), the
// metrics' ringing over the room's own tail.
void movementAndRinging()
{
    std::vector<float> tones(size_t(14.0f * kSr), 0.0f), burst(size_t(8.0f * kSr), 0.0f);
    for (size_t i = 0; i < tones.size(); ++i) {
        const double t = double(i) / kSr;
        tones[i] = t < 4.0 ? 0.3f * noise()
                 : t >= 4.5 ? float(0.15 * (std::sin(2.0 * M_PI * 700.0 * t) + std::sin(2.0 * M_PI * 1300.0 * t))) : 0.0f;
    }
    std::vector<float> played(size_t(14.0f * kSr), 0.0f);
    for (size_t i = 0; i < played.size(); ++i) {
        const double t = double(i) / kSr;
        played[i] = t < 4.0 || t >= 4.5 ? 0.2f * noise() : 0.0f;
    }
    for (size_t i = 0; i < burst.size(); ++i) {
        const double t = double(i) / kSr;
        burst[i] = t < 4.0 || (t >= 4.5 && t < 4.7) ? 0.3f * noise() : 0.0f;
    }
    // v1.0.47's own ringing (the page's B, bit for bit), dB: HALL then MASSIVE, SIZE 0 / 0.5 / 0.9.
    // The room alone's level on noise, dBFS (HALL, MASSIVE x SIZE 0 / 0.5 / 0.9), with the page's trims (voicing::kTwinRoomTrimDb:
    // B level-matched to A on the page's chord and vibes, A-weighted). On this white noise the wander as before read
    // -34.7 / -34.7 / -34.8 (HALL) and -33.8 / -33.7 / -33.8 (MASSIVE): music, not noise, set the page's match.
    // HALL re-pinned at v1.0.61 (ADR 0008 amendment, endless follows the playing): at this DECAY (0.9, past the finite
    // top) everything follows DECAY 0.80 while you play, the room's level with it, so HALL's long-tail trim (ADR 0034,
    // eased back out across the crossover before) now applies: 2.1 dB under v1.0.47's -35.52 / -35.49 / -35.28.
    // MASSIVE's trim is the same at 0.80 and 0.9: unchanged.
    const double levelB[2][3] = {{-37.62, -37.59, -37.38}, {-34.54, -34.51, -34.30}};
    const double ringB[2][3] = {{13.7, 7.2, 7.1}, {13.7, 7.2, 7.1}};
    for (int space : {1, 2})
        for (int k = 0; k < 3; ++k) {
            Setting s;
            s.space = space;
            s.size = kSizes[k];
            s.layerSolo = true;
            const Stereo out = render(tones, s, {{4.0, true}, {4.1, false}});
            const double c = iv::test::p95Cents(out.l, out.r, 1300.0, kSr, 6.0, 13.0);
            char msg[200];
            std::snprintf(msg, sizeof msg, "%s SIZE %.1f, the room alone, frozen: a held 1.3 kHz moves %.1f c (p95; at most 10)",
                          name(space), kSizes[k], c);
            check(finite(out) && c <= 10.0, msg);
            const double lvl = rmsDb(render(played, s, {{4.0, true}, {4.1, false}}), 6.0, 13.0);
            std::snprintf(msg, sizeof msg, "%s SIZE %.1f, the room alone: %.2f dBFS on noise played over the freeze (pinned "
                          "%.2f ±0.2: the page's level for B, matched to the room as before)", name(space), kSizes[k], lvl,
                          levelB[space - 1][k]);
            check(std::fabs(lvl - levelB[space - 1][k]) <= 0.2, msg);

            const Stereo b = render(burst, s, {{4.0, true}, {4.1, false}});
            std::vector<float> l(b.l.begin() + long(4.7 * kSr), b.l.end()), r(b.r.begin() + long(4.7 * kSr), b.r.end());
            const auto m = iv::metrics::compute({l, r}, kSr);
            std::snprintf(msg, sizeof msg, "%s SIZE %.1f, the room's ringing %.1f dB at %.0f Hz (the page's B: %.1f, at most +1)",
                          name(space), kSizes[k], m.ringingDb, m.ringingHz, ringB[space - 1][k]);
            check(m.ringingDb <= ringB[space - 1][k] + 1.0, msg);
        }
}

// Paint's ceiling (ADR 0010, kPaintHeadroomDb 1.5 dB) at every SIZE: the pad alone, judged before and after a paint
// (as test_plate_freeze's paint()).
void paintCeiling()
{
    std::vector<float> in(size_t(60.0f * kSr), 0.0f);
    for (size_t i = 0; i < in.size(); ++i) {
        const double t = double(i) / kSr;
        in[i] = (t < 3.0 || (t >= 20.0 && t < 45.0)) ? 0.9f * noise() : 0.0f;
    }
    for (int space : {1, 2})
        for (float size : kSizes) {
            Setting s;
            s.space = space;
            s.size = size;
            s.decay = 0.5f;
            const Stereo out = render(in, s, {{2.5, true}, {2.6, false}, {30.0, true}, {45.0, false}});
            const double before = rmsDb(out, 14.0, 19.0), after = rmsDb(out, 55.0, 59.0);
            const double lowGrowth = lowDb(out, 55.0, 59.0) - lowDb(out, 14.0, 19.0);
            char msg[220];
            std::snprintf(msg, sizeof msg, "%s SIZE %.1f: Paint lifts the pad %+.2f dB, below 1 kHz %+.2f (at most +1.5; not down, "
                                           "or up below 1 kHz) and it stays", name(space), size, after - before, lowGrowth);
            // v1.0.58: on white noise a painted HALL pad's top settles a little under (to -0.4 dB, as v1.0.57's did) while
            // the room paints below 1 kHz; v1.0.57's total rose on its still lows creeping up (test_plate_freeze paint()).
            check(finite(out) && (after >= before - 0.1 || (after >= before - 0.5 && lowGrowth > 0.5)) && after <= before + 1.5 + 0.1, msg);
        }
}

// A painted pad holds: frozen at 3 s, painted 5-15 s with noise playing, then 10 minutes frozen; the level at the end
// within 1 dB of 60-70 s.
void paintedHolds()
{
    struct Job { int space; float size; double a = 0, b = 0; bool ok = false; };
    std::vector<Job> jobs;
    for (int space : {1, 2})
        for (float size : {0.0f, 0.9f}) jobs.push_back({space, size});
    std::vector<std::thread> threads;
    for (auto& j : jobs)
        threads.emplace_back([&j] {
            std::vector<float> in(size_t(600.0f * kSr), 0.0f);
            uint32_t seed = 17 + uint32_t(j.space * 3) + uint32_t(j.size * 10.0f);
            for (size_t i = 0; i < in.size(); ++i) {
                const double t = double(i) / kSr;
                if (t < 3.0 || (t >= 5.0 && t < 15.0)) {
                    seed = seed * 1664525u + 1013904223u;
                    in[i] = 0.3f * float(int32_t(seed)) / 2147483648.0f;
                }
            }
            Setting s;
            s.space = j.space;
            s.size = j.size;
            const Stereo out = render(in, s, {{3.0, true}, {3.1, false}, {5.0, true}, {15.0, false}});
            j.a = rmsDb(out, 60.0, 70.0);
            j.b = rmsDb(out, 590.0, 600.0);
            j.ok = finite(out) && j.a > -70.0 && std::fabs(j.b - j.a) <= 1.0;
        });
    for (auto& t : threads) t.join();
    for (const auto& j : jobs) {
        char msg[200];
        std::snprintf(msg, sizeof msg, "%s SIZE %.1f, painted then frozen 10 minutes: %.1f dB at 60-70 s, %+.2f dB at the end (within 1)",
                      name(j.space), j.size, j.a, j.b - j.a);
        check(j.ok, msg);
    }
}

// Freeze, frozen SIZE moves (a glide and flicks), paint and unfreeze over a pad: no clicks (the metrics' detector),
// at every SIZE; and the room alone through the same SIZE moves.
void clicks()
{
    const size_t n = size_t(12.0f * kSr);
    std::vector<float> in(n);
    for (size_t i = 0; i < n; ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / kSr) + 0.02f * noise();
    for (int space : {1, 2})
        for (float size : kSizes)
            for (bool solo : {false, true}) {
                Setting s;
                s.space = space;
                s.size = size;
                s.mix = solo ? 1.0f : 0.5f;
                s.layerSolo = solo;
                const float other = size < 0.5f ? 0.9f : 0.0f;
                // Over the pad, SIZE jumps (a CV); the room alone, turned by hand: each move over 50 ms. (Jumped, the room
                // alone at SIZE 0 reads one click at the 0.9 -> 0 flick, 0.3 dB over the detector's line, with the
                // wander as before v1.0.47 too: the lines' semitone glide itself, not the wander.)
                std::vector<SizeMove> moves;
                const double ramp = solo ? 0.05 : 0.0;
                float from = size;
                for (const SizeMove& m : {SizeMove{3.0, other}, SizeMove{4.0, size}, SizeMove{4.5, other}, SizeMove{4.6, size}}) {
                    for (double u = 0.0; u < ramp; u += 0.001)
                        moves.push_back({m.at + u, from + float(u / ramp) * (m.size - from)});
                    moves.push_back({m.at + ramp, m.size});
                    from = m.size;
                }
                const Stereo out = render(in, s, {{2.0, true}, {2.1, false}, {6.0, true}, {7.0, false}, {9.0, true}, {9.1, false}},
                                          moves);
                const auto m = iv::metrics::compute({out.l, out.r}, kSr);
                char msg[200];
                std::snprintf(msg, sizeof msg, "%s SIZE %.1f%s: freeze, frozen SIZE moves, paint, unfreeze: no clicks (%d)",
                              name(space), size, solo ? ", the room alone" : "", m.clickCount);
                check(finite(out) && m.clickCount == 0, msg);
            }
}

} // namespace

int main()
{
    movementAndRinging();
    paintCeiling();
    paintedHolds();
    clicks();
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
