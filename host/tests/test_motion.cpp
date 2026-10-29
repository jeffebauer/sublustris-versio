// v1.0.46: MOTION (key "drift"), the owner's picks on renders/drift_rethink. Left side: the tape transport on the wet
// (dsp/Transport.h: wow and flutter with more variance at the far end, Magneto crinkle, a third of round 2's crinkle
// dips), the tank's own per-line wobble cut to a third; right side: Wander stronger over its last half (B).
// Pins: noon and the dead zone untouched (bit for bit, the transport resting); the left side's depth (p95 cents of a
// held 1.3 kHz, the page's own measure) and its crinkle and dip counts at 9 o'clock, 3/4 left and full left to the
// confirm page's B (left_tape_final/_verify.txt) within 10 %; the right side's depth; the transport's latency and its
// glide out back to rest; no clicks on MOTION sweeps and flicks, nor from the dips (1 ms frames of the highs against
// their neighbours); no energy growth, and 10-minute infinite holds at DECAY 1 at both ends in every SPACE (the
// per-line change is in the loop); music's loudness within ±1 dB of noon across the knob.

#include "DecayV1052.h"
#include "PitchTrack.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "dsp/Transport.h"
#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;
constexpr float kSr = 48000.0f;
const char* kSpaceName[3] = {"PLATE", "HALL", "MASSIVE"};

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

struct Stereo {
    std::vector<float> l, r;
};

struct Setting {
    int   space = 1;
    float drift = 0.5f, decay = 0.7f, size = 0.6f;
};

// The Engine on in (mono, both inputs), 48-sample blocks, the page's settings (ERA 0, NORMAL, TIME 0, MIX 100 %);
// driftAt(t) sets MOTION each block (a CV).
template <typename DriftAt>
Stereo render(const std::vector<float>& in, const Setting& s, DriftAt driftAt, iv::Engine* keep = nullptr)
{
    iv::Engine local;
    iv::Engine& e = keep ? *keep : local;
    e.prepare(kSr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Decay, s.decay);
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Era, 0.0f);
    e.setParam(iv::ParamId::Mix, 1.0f);
    e.setParam(iv::ParamId::Time, 0.0f);
    e.setParam(iv::ParamId::Drift, driftAt(0.0));
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        e.setParam(iv::ParamId::Drift, driftAt(double(pos) / kSr));
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}
Stereo render(const std::vector<float>& in, const Setting& s, iv::Engine* keep = nullptr)
{
    const float d = s.drift;
    return render(in, s, [d](double) { return d; }, keep);
}

Setting pageSetting(int space, float drift) // the page's SPACEs: SIZE 0.6 (MASSIVE 0.7), DECAY 0.7
{
    Setting s;
    s.space = space;
    s.size = space == 2 ? 0.7f : 0.6f;
    s.drift = drift;
    s.decay = decaySameTailAsV1052(0.7f, space); // the page's tail length (v1.0.53: PLATE / HALL tops 30 / 60 s)
    return s;
}

double p95Of(const Stereo& s) { return iv::test::p95Cents(s.l, s.r, 1300.0, kSr); } // 1.3 kHz, 3-9 s

std::vector<float> heldTones() // the page's tones.wav: 9 s of 700 Hz + 1.3 kHz, 24-bit
{
    std::vector<float> x(size_t(9.0f * kSr));
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = double(i) / kSr;
        double v = 0.15 * (std::sin(2.0 * M_PI * 700.0 * t) + std::sin(2.0 * M_PI * 1300.0 * t));
        if (i >= x.size() - 2400) v *= 1.0 - double(i - (x.size() - 2400)) / 2399.0;
        x[i] = float(std::round(v * 8388608.0) / 8388608.0);
    }
    return x;
}

// ---- Checks ------------------------------------------------------------------------------------------------

void deadZone()
{
    std::vector<float> in(size_t(6.0f * kSr), 0.0f);
    uint32_t seed = 5;
    for (size_t i = 0; i < size_t(1.0f * kSr); ++i) {
        seed = seed * 1664525u + 1013904223u;
        in[i] = 0.3f * float(int32_t(seed)) / 2147483648.0f;
    }
    for (int space : {0, 1, 2}) {
        const Stereo noon = render(in, pageSetting(space, 0.5f));
        bool same = true, rests = true;
        for (float d : {iv::voicing::kDriftDeadLo, iv::voicing::kDriftDeadHi}) {
            iv::Engine e;
            const Stereo edge = render(in, pageSetting(space, d), &e);
            same = same && std::memcmp(noon.l.data(), edge.l.data(), noon.l.size() * sizeof(float)) == 0
                   && std::memcmp(noon.r.data(), edge.r.data(), noon.r.size() * sizeof(float)) == 0;
            rests = rests && e.transport().idle();
        }
        check(same && rests, std::string(kSpaceName[space]) + ": MOTION's dead zone (noon to its edges) renders bit for bit "
                                 "as noon, the tape transport resting");
    }
}

// The wet's first sound at full left lands kBase samples after noon's (the transport's centre: the left side's latency).
void latency()
{
    std::vector<float> in(size_t(0.5f * kSr), 0.0f);
    in[0] = 1.0f;
    auto onset = [](const Stereo& s) {
        for (size_t i = 0; i < s.l.size(); ++i)
            if (std::fabs(s.l[i]) > 1e-4f) return long(i);
        return -1L;
    };
    const long a = onset(render(in, pageSetting(1, 0.5f))), b = onset(render(in, pageSetting(1, 0.0f)));
    char msg[200];
    std::snprintf(msg, sizeof msg, "MOTION full left: the wet arrives %ld samples (%.1f ms) after noon's (the transport's "
                  "centre, %d; within 4)", b - a, double(b - a) / 48.0, int(iv::dsp::Transport::kBase));
    check(a > 0 && std::labs(b - a - long(iv::dsp::Transport::kBase)) <= 4, msg);
}

// Turned back to noon, the transport glides out and rests (the wet as without it again).
void glideOut()
{
    std::vector<float> in(size_t(20.0f * kSr), 0.0f);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.1f * std::sin(6.2831853f * 440.0f * float(i) / kSr);
    iv::Engine e;
    bool restedAt6 = false, restedAt10 = false;
    float centreMid = 0.0f;
    render(in, pageSetting(1, 0.5f), [&](double t) {
        if (t > 8.0 && t < 8.01) centreMid = e.transport().centre();
        if (t > 14.0 && t < 14.01) restedAt6 = e.transport().idle(); // 6 s after the return
        if (t > 18.0 && t < 18.01) restedAt10 = e.transport().idle();
        return t < 2.0 ? 0.5f : t < 8.0 ? 0.0f : 0.5f;
    }, &e);
    char msg[200];
    std::snprintf(msg, sizeof msg, "MOTION noon -> full left -> noon: the transport's centre reached %.0f samples, and "
                  "it rests again within 6 s of the return", centreMid);
    check(centreMid == iv::dsp::Transport::kBase && restedAt6 && restedAt10, msg);
}

// The left side against the confirm page's B (left_tape_final/_verify.txt): p95 cents and the transport's counts.
// HALL's p95s are v1.0.57's own (ADR 0035: in this long tail HALL's floor is twice as deep, so a held note moves more
// with MOTION too; the page's B: 9.2 / 12.6 / 18.5). v1.0.59 (motion_dips C): the dips come in from ~7:30
// (kTransportDipsFrom), so 3/4 left has none (the page's B: 6); its crinkle and warbles are unchanged.
void leftSide()
{
    const std::vector<float> tones = heldTones();
    struct Pos { float drift; const char* name; double p95[3]; int crinkles, dips; }; // p95: PLATE, HALL, MASSIVE
    const Pos pos[] = {{0.2f, "9 o'clock (0.2)", {13.8, 15.3, 5.7}, 0, 0},
                       {0.1175f, "3/4 left (0.1175)", {16.0, 15.2, 9.6}, 13, 0},
                       {0.0f, "full left", {19.9, 19.8, 15.1}, 34, 15}};
    for (const Pos& p : pos) {
        for (int space : {0, 1, 2}) {
            const double c = p95Of(render(tones, pageSetting(space, p.drift)));
            char msg[200];
            std::snprintf(msg, sizeof msg, "MOTION %s, %s: p95 %.1f c on a held 1.3 kHz (%s: %.1f, within 10 %%)",
                          p.name, kSpaceName[space], c, space == 1 ? "v1.0.57" : "the page's B", p.p95[space]);
            check(std::fabs(c / p.p95[space] - 1.0) <= 0.10, msg);
        }
        std::vector<float> minute(size_t(60.0f * kSr), 0.0f);
        iv::Engine e;
        render(minute, pageSetting(1, p.drift), &e);
        const int cr = e.transport().crinkles(), di = e.transport().dips();
        char msg[200];
        std::snprintf(msg, sizeof msg, "MOTION %s: %d crinkle bursts and %d dips a minute (the page's B: %d and %d, within 10 %%)",
                      p.name, cr, di, p.crinkles, p.dips);
        check(std::abs(cr - p.crinkles) <= std::max(0, int(std::floor(0.1 * p.crinkles)))
                  && std::abs(di - p.dips) <= std::max(0, int(std::floor(0.1 * p.dips))), msg);
    }
    // The dips' share (v1.0.59): none from kTransportDipsFrom up, rising straight to kTransportDipShare at full left.
    using namespace iv::voicing;
    const float from = kTransportDipsFrom;
    check(iv::map::transportAmounts(from).crinkleDip == 0.0f && iv::map::transportAmounts(0.188f).crinkleDip == 0.0f
              && iv::map::transportAmounts(0.0f).crinkleDip == kTransportDipShare
              && std::fabs(iv::map::transportAmounts(0.5f * from).crinkleDip - 0.5f * kTransportDipShare) < 1e-6f,
          "MOTION's tape dips: none from ~7:30 (MOTION 0.026) up, half the share half way, the full share fully left");
}

// The right side: Wander, stronger over its last half (B). 3/4 right is unchanged by B, so it is pinned to the round-2
// right page's measure of today's (docs/research/drift-rethink.md: 14.0 / 11.9 / 8.1); full right to v1.0.46's own
// figures (the picked B, bit for bit against the prototype), and deeper than today's full right (17.9 / 20.3 / 13.1).
// HALL's are v1.0.57's (ADR 0035, its floor twice as deep in this long tail; were 11.9 and 22.4).
void rightSide()
{
    const std::vector<float> tones = heldTones();
    const double pinned[2][3] = {{14.0, 14.7, 8.1}, {23.6, 23.6, 17.3}}; // 3/4 right, full right: PLATE, HALL, MASSIVE
    const double before[3] = {17.9, 20.3, 13.1};                         // full right before v1.0.46
    const float drifts[2] = {0.75f, 1.0f}; // the right page's positions
    for (int k = 0; k < 2; ++k)
        for (int space : {0, 1, 2}) {
            const double c = p95Of(render(tones, pageSetting(space, drifts[k])));
            char msg[220];
            if (k == 0)
                std::snprintf(msg, sizeof msg, "MOTION 3/4 right (0.75), %s: p95 %.1f c (as before v1.0.46: %.1f, within 10 %%)",
                              kSpaceName[space], c, pinned[k][space]);
            else
                std::snprintf(msg, sizeof msg, "MOTION full right, %s: p95 %.1f c (v1.0.46: %.1f, within 10 %%; before: %.1f)",
                              kSpaceName[space], c, pinned[k][space], before[space]);
            check(std::fabs(c / pinned[k][space] - 1.0) <= 0.10 && (k == 0 || c > before[space]), msg);
        }
    // The mapping itself: x 1.4 deeper and x 1.2 quicker at the end, the side's first half as before.
    const auto end = iv::map::driftAmounts(1.0f), mid = iv::map::driftAmounts(0.53f + 0.5f * 0.47f);
    check(std::fabs(end.wowCents / iv::voicing::kWanderCents - 1.4f) < 1e-4f
              && std::fabs(end.wowHz / iv::voicing::kWanderHzMax - 1.2f) < 1e-4f
              && std::fabs(mid.wowCents / (0.5f * iv::voicing::kWanderCents) - 1.0f) < 1e-4f,
          "MOTION's right side: Wander x1.4 deeper and x1.2 quicker at full, as before at the side's middle");
    const auto left = iv::map::driftAmounts(0.0f);
    check(std::fabs(left.wowCents - iv::voicing::kWowCents * iv::voicing::kTransportLineShare) < 1e-4f,
          "MOTION's left side: the tank's lines keep a third of their wobble (the transport carries the rest)");
}

// The worst 1 ms frame of the highs (> 5 kHz, two RBJ high-passes) against its neighbours 2-4 ms each side, dB
// (test_tone's measure); frames under -90 dBFS are left out.
double spikeDb(const std::vector<float>& l, const std::vector<float>& r, double from)
{
    double z[2][2] = {}, y[2][2] = {};
    const double w0 = 6.283185307 * 5000.0 / kSr, al = std::sin(w0) / (2.0 * 0.7071), c = std::cos(w0);
    const double b0 = (1 + c) / 2 / (1 + al), b1 = -(1 + c) / (1 + al), b2 = b0, a1 = -2 * c / (1 + al), a2 = (1 - al) / (1 + al);
    std::vector<double> e;
    double acc = 0.0;
    for (size_t i = 0; i < l.size(); ++i) {
        double v = 0.5 * (double(l[i]) + r[i]);
        for (int st = 0; st < 2; ++st) {
            const double o = b0 * v + b1 * z[st][0] + b2 * z[st][1] - a1 * y[st][0] - a2 * y[st][1];
            z[st][1] = z[st][0]; z[st][0] = v;
            y[st][1] = y[st][0]; y[st][0] = o;
            v = o;
        }
        acc += v * v;
        if ((i + 1) % 48 == 0) { e.push_back(acc / 48.0); acc = 0.0; }
    }
    double worst = -99.0;
    for (size_t i = size_t(from * 1000.0) + 4; i + 5 < e.size(); ++i) {
        if (e[i] < 1e-9) continue;
        const double near = (e[i - 4] + e[i - 3] + e[i - 2] + e[i + 2] + e[i + 3] + e[i + 4]) / 6.0;
        worst = std::fmax(worst, 10.0 * std::log10(e[i] / (near + 1e-30)));
    }
    return worst;
}

bool skank(std::vector<float>& in)
{
    iv::wav::Audio a;
    std::string error;
    bool read = false;
    for (const char* prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (!read) read = iv::wav::read(std::string(prefix) + "04_skank.wav", a, error);
    if (!read) return false;
    in = a.channels[0];
    return true;
}

void sweeps(const std::vector<float>& sk)
{
    struct Move { const char* name; double secs; double (*at)(double); };
    const Move moves[] = {
        {"a 0.1 Hz sweep", 11.0, [](double t) { const double p = std::fmod(t * 0.1 + 0.25, 1.0); return p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p; }},
        {"a 2 Hz sweep", 9.0, [](double t) { const double p = std::fmod(t * 2.0 + 0.25, 1.0); return p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p; }},
        {"flicks noon <-> full left", 9.0, [](double t) { return std::fmod(t, 1.0) < 0.5 ? 0.5 : 0.0; }},
        {"flicks noon <-> full right", 9.0, [](double t) { return std::fmod(t, 1.0) < 0.5 ? 0.5 : 1.0; }},
    };
    for (int space : {0, 1, 2})
        for (const Move& m : moves) {
            std::vector<float> in(sk.begin(), sk.begin() + long(std::min(sk.size(), size_t(m.secs * kSr))));
            auto at = m.at;
            const Stereo s = render(in, pageSetting(space, 0.5f), [at](double t) { return t < 1.0 ? 0.5f : float(at(t - 1.0)); });
            const Stereo n = render(in, pageSetting(space, 0.5f));
            const double on = spikeDb(s.l, s.r, 1.0), off = spikeDb(n.l, n.r, 1.0);
            char msg[200];
            std::snprintf(msg, sizeof msg, "%s, MOTION %s: worst 1 ms spike %.1f dB, %.1f at noon (at most +3)", kSpaceName[space],
                          m.name, on, off);
            check(on <= off + 3.0, msg);
        }
}

// The dips themselves: the transport at full left on a held organ chord (pure tones, where a click shows most),
// with its dips against the same transport without them (the same wow and bursts: the dips' random numbers are their own).
void dipClicks()
{
    std::vector<float> chord(size_t(60.0f * kSr));
    for (size_t i = 0; i < chord.size(); ++i) {
        const double t = double(i) / kSr;
        double v = 0.0;
        for (double f : {261.6, 329.6, 392.0, 523.3, 1046.5, 2093.0}) v += 0.06 * std::sin(2.0 * M_PI * f * t);
        chord[i] = float(v);
    }
    double res[2];
    int dips[2];
    for (int k = 0; k < 2; ++k) {
        iv::dsp::Transport tp;
        tp.prepare(kSr, 0x7a9e5u);
        tp.reset(true);
        auto a = iv::map::transportAmounts(0.0f);
        if (k == 1) a.crinkleDip = 0.0f;
        std::vector<float> l(chord), r(chord);
        for (size_t pos = 0; pos < l.size(); pos += 32) {
            tp.control(32, a, true);
            for (size_t i = pos; i < pos + 32 && i < l.size(); ++i) tp.process(l[i], r[i]);
        }
        res[k] = spikeDb(l, r, 1.0);
        dips[k] = tp.dips();
    }
    char msg[200];
    std::snprintf(msg, sizeof msg, "MOTION full left, a held organ chord: worst 1 ms spike %.1f dB with the dips (%d a minute), "
                  "%.1f without (at most +3)", res[0], dips[0], res[1]);
    check(dips[0] >= 13 && dips[1] == 0 && res[0] <= res[1] + 3.0, msg);
}

double rmsDb(const std::vector<float>& l, const std::vector<float>& r, double from, double to)
{
    const size_t a = size_t(from * kSr), b = std::min(size_t(to * kSr), l.size());
    double sum = 0.0;
    for (size_t i = a; i < b; ++i) sum += double(l[i]) * l[i] + double(r[i]) * r[i];
    return 10.0 * std::log10(sum / double(2 * (b - a)) + 1e-30);
}

// No energy growth: a noise burst's tail at DECAY 0.75, SIZE 1, MOTION at its ends against noon, window by window.
void energy()
{
    std::vector<float> in(size_t(30.0f * kSr), 0.0f);
    uint32_t seed = 9;
    for (size_t i = 0; i < size_t(1.0f * kSr); ++i) {
        seed = seed * 1664525u + 1013904223u;
        in[i] = 0.3f * float(int32_t(seed)) / 2147483648.0f;
    }
    for (int space : {0, 1, 2}) {
        Setting s;
        s.space = space;
        s.decay = 0.75f;
        s.size = 1.0f;
        const Stereo noon = render(in, s);
        double worst = -99.0;
        std::string detail;
        for (float d : {0.0f, 0.1175f, 0.75f, 1.0f}) {
            s.drift = d;
            const Stereo m = render(in, s);
            for (double w0 : {2.0, 5.0, 10.0, 20.0}) {
                const double w1 = w0 < 10.0 ? w0 * 2.0 : w0 + 10.0;
                const double ref = rmsDb(noon.l, noon.r, w0, w1);
                if (ref < -100.0) continue; // the floor's numerical noise
                worst = std::fmax(worst, rmsDb(m.l, m.r, w0, w1) - ref);
            }
        }
        char msg[200];
        std::snprintf(msg, sizeof msg, "%s, DECAY 0.75, SIZE 1: MOTION's ends add no energy to the tail (worst window %+.2f dB "
                      "against noon, at most +0.5)", kSpaceName[space], worst);
        check(worst <= 0.5, msg);
    }
}

// 10-minute infinite holds (DECAY 1) at both ends, every SPACE: finite, and the level at the end within 1.5 dB of
// its level 60-70 s in, once the hold has settled (v1.0.58; was 10-20 s, as test_room_motion's painted pads are judged):
// after a loud burst stops, an endless tail dips while its hold is taken and the make-up brings it back over the next
// ~30 s, in HALL 2.4-2.8 dB since v1.0.58 (v1.0.57 1.3-1.4: its still lows took the make-up meant for the main tank);
// that rise is held to 3.5 dB.
void holds()
{
    struct Job { int space; float drift; double a = 0, b = 0, early = 0; bool ok = false; };
    std::vector<Job> jobs;
    for (int space : {0, 1, 2})
        for (float d : {0.0f, 1.0f}) jobs.push_back({space, d});
    std::vector<std::thread> threads;
    for (auto& j : jobs)
        threads.emplace_back([&j] {
            const size_t n = size_t(600.0f * kSr);
            std::vector<float> in(n, 0.0f);
            uint32_t seed = 11;
            for (size_t i = 0; i < size_t(3.0f * kSr); ++i) {
                seed = seed * 1664525u + 1013904223u;
                in[i] = 0.2f * float(int32_t(seed)) / 2147483648.0f;
            }
            Setting s;
            s.space = j.space;
            s.drift = j.drift;
            s.decay = 1.0f;
            s.size = 0.6f;
            const Stereo out = render(in, s);
            bool fin = true;
            for (size_t i = 0; i < n && fin; ++i) fin = std::isfinite(out.l[i]) && std::isfinite(out.r[i]);
            j.early = rmsDb(out.l, out.r, 10.0, 20.0);
            j.a = rmsDb(out.l, out.r, 60.0, 70.0);
            j.b = rmsDb(out.l, out.r, 590.0, 600.0);
            j.ok = fin && j.a > -60.0 && std::fabs(j.b - j.a) <= 1.5 && std::fabs(j.b - j.early) <= 3.5;
        });
    for (auto& t : threads) t.join();
    for (const auto& j : jobs) {
        char msg[260];
        std::snprintf(msg, sizeof msg, "%s infinite, MOTION %s: 10 minutes hold (%.1f dB at 60-70 s, %+.2f dB at the end; within 1.5; "
                      "%+.2f from 10-20 s, within 3.5)", kSpaceName[j.space], j.drift == 0.0f ? "full left" : "full right", j.a, j.b - j.a,
                      j.b - j.early);
        check(j.ok, msg);
    }
}

// BS.1770's K-weighting at 48 kHz, the whole render (as test_tone's).
double kWeightedDb(const Stereo& s)
{
    double sum = 0.0;
    for (const auto* ch : {&s.l, &s.r}) {
        double a1 = 0, a2 = 0, b1 = 0, b2 = 0;
        for (float x : *ch) {
            const double w = x + 1.69065929318241 * a1 - 0.73248077421585 * a2;
            const double y = 1.53512485958697 * w - 2.69169618940638 * a1 + 1.19839281085285 * a2;
            a2 = a1; a1 = w;
            const double v = y + 1.99004745483398 * b1 - 0.99007225036621 * b2;
            const double k = v - 2.0 * b1 + b2;
            b2 = b1; b1 = v;
            sum += k * k;
        }
    }
    return 10.0 * std::log10(sum / double(2 * s.l.size()) + 1e-30) - 0.691;
}

void level(const std::vector<float>& sk)
{
    std::vector<float> in(sk);
    in.resize(in.size() + size_t(4.0f * kSr), 0.0f);
    for (int space : {0, 1, 2}) {
        const double noon = kWeightedDb(render(in, pageSetting(space, 0.5f)));
        double worst = 0.0;
        std::string detail;
        for (float d : {0.0f, 0.1175f, 0.2f, 0.75f, 1.0f}) {
            const double dd = kWeightedDb(render(in, pageSetting(space, d))) - noon;
            if (std::fabs(dd) > std::fabs(worst)) worst = dd;
            char buf[32];
            std::snprintf(buf, sizeof buf, " %.3f: %+.2f", d, dd);
            detail += buf;
        }
        check(std::fabs(worst) <= 1.0, std::string(kSpaceName[space]) + ": music's loudness (LUFS) within ±1 dB of noon across MOTION:" + detail);
    }
}

} // namespace

int main()
{
    deadZone();
    latency();
    glideOut();
    leftSide();
    rightSide();
    dipClicks();
    std::vector<float> sk;
    if (!skank(sk)) {
        check(false, "MOTION on music: test_audio/stimulus/04_skank.wav (python3 tools/make_stimulus.py)");
    } else {
        sweeps(sk);
        level(sk);
    }
    energy();
    holds();
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
