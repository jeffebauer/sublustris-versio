// M1: the FDN Hall (SPEC §4.3, §7 M1). Stability at every DECAY × SIZE ×
// TONE corner, T60 against the DECAY mapping, no metallic ringing, SIZE
// and TIME move by crossfade (ADR 0025: pitch kept, no sweep, no clicks), determinism, the
// same tail at 44.1 / 48 / 96 kHz, and true stereo in (ADR 0013).

#include "Metrics.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

struct Stereo {
    std::vector<float> l, r;
};

struct Setting {
    float decay = 0.5f, size = 0.5f, tone = 0.5f, mix = 1.0f;
};

uint32_t gSeed = 7;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}

// Mono stimulus on both inputs unless rightSilent.
Stereo render(const std::vector<float>& in, const Setting& s, float sampleRate = 48000.0f, bool rightSilent = false,
              const std::vector<float>* sizeRamp = nullptr)
{
    iv::Engine e;
    e.prepare(sampleRate, 48);
    e.setParam(iv::ParamId::Decay, s.decay);
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Tone, s.tone);
    e.setParam(iv::ParamId::Mix, s.mix);
    e.setParam(iv::ParamId::Time, 0.0f);
    e.setParam(iv::ParamId::Era, 0.0f); // the Neve alone (as ERA's default was until v1.0.42): the wear's tape compression takes more loss on a hot tank
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    const std::vector<float> silent(in.size(), 0.0f);
    const float* right = rightSilent ? silent.data() : in.data();
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        if (sizeRamp) e.setParam(iv::ParamId::Size, (*sizeRamp)[pos]);
        e.process(in.data() + pos, right + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

double rmsDb(const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to && i < x.size(); ++i) sum += double(x[i]) * x[i];
    return 10.0 * std::log10(sum / double(to - from) + 1e-30);
}

bool finite(const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}

float peak(const Stereo& s)
{
    float p = 0.0f;
    for (size_t i = 0; i < s.l.size(); ++i) p = std::fmax(p, std::fmax(std::fabs(s.l[i]), std::fabs(s.r[i])));
    return p;
}

// Half a second of noise at about −12 dBFS, then silence.
std::vector<float> burst(float sampleRate, float seconds)
{
    std::vector<float> x(size_t(seconds * sampleRate), 0.0f);
    for (size_t i = 0; i < size_t(0.5f * sampleRate); ++i) x[i] = 0.25f * noise();
    return x;
}

// Stable everywhere: finite, bounded, and a finite DECAY dies away while an
// infinite one holds without growing.
void stabilityGrid()
{
    const float sr = 48000.0f;
    const auto in = burst(sr, 8.0f);
    const float decays[] = {0.0f, 0.4f, 0.8f, 0.85f, 1.0f};
    const float sizes[] = {0.0f, 0.5f, 1.0f};
    const float tones[] = {0.0f, 0.5f, 1.0f};
    int bad = 0, total = 0;
    for (float d : decays)
        for (float z : sizes)
            for (float t : tones) {
                ++total;
                const Stereo out = render(in, {d, z, t});
                const double early = rmsDb(out.l, size_t(1.0f * sr), size_t(2.0f * sr));
                const double late = rmsDb(out.l, size_t(7.0f * sr), size_t(8.0f * sr));
                bool ok = finite(out) && peak(out) < 2.0f;
                if (d < iv::map::kDecayFiniteTop + 1e-3f) ok &= late < early - 3.0 || early < -200.0; // or already silent
                else ok &= late < early + 1.0; // infinite: holds, never grows
                if (!ok) {
                    ++bad;
                    std::printf("      decay %.2f size %.1f tone %.1f: peak %.2f, 1-2 s %.1f dB, 7-8 s %.1f dB\n", d, z, t,
                                peak(out), early, late);
                }
            }
    check(bad == 0, "stable at all " + std::to_string(total) + " DECAY x SIZE x TONE corners (finite, bounded, no growth)");
}

// The low band's T60 against DECAY's mapping (the highs are meant to die
// sooner: that's the damping). Within 10 % (SPEC §7 M1).
double lowBandT60(const Stereo& out, float sr)
{
    std::vector<float> lp(out.l.size());
    const float c = 1.0f - std::exp(-2.0f * 3.14159265f * 500.0f / sr);
    float a = 0.0f, b = 0.0f;
    for (size_t i = 0; i < lp.size(); ++i) {
        a += c * (0.5f * (out.l[i] + out.r[i]) - a);
        b += c * (a - b);
        lp[i] = b;
    }
    return iv::metrics::compute({lp}, sr).t60S;
}

void t60FollowsDecay()
{
    const float sr = 48000.0f;
    for (float d : {0.2f, 0.5f, 0.75f}) {
        const double want = iv::map::hallT60Scale(0.5f) / iv::map::decayRate(d, 1); // SIZE scales a HALL's T60 (M1 notes)
        std::vector<float> in(size_t((2.0 * want + 3.0) * sr), 0.0f);
        in[100] = 0.5f;
        const Stereo out = render(in, {d, 0.5f, 0.5f});
        const double got = lowBandT60(out, sr);
        char msg[160];
        std::snprintf(msg, sizeof msg, "T60 at DECAY %.2f: %.2f s, mapping %.2f s (within 10%%)", d, got, want);
        check(std::isfinite(got) && std::fabs(got / want - 1.0) < 0.10, msg);
    }
}

// SPEC §7 M1 metallic check: no narrow peak keeps outliving its neighbours.
void notMetallic()
{
    const float sr = 48000.0f;
    std::vector<float> in(size_t(10.0f * sr), 0.0f);
    for (int hit = 0; hit < 3; ++hit)
        for (size_t i = 0; i < 1200; ++i) in[size_t(hit) * 24000 + i] = 0.6f * noise() * std::exp(-float(i) / 200.0f);
    for (float d : {0.0f, 0.4f, 0.75f})
        for (float z : {0.0f, 1.0f})
            for (float t : {0.5f, 1.0f}) {
                const Stereo out = render(in, {d, z, t});
                const auto m = iv::metrics::compute({out.l, out.r}, sr);
                char msg[200];
                std::snprintf(msg, sizeof msg, "not metallic at DECAY %.2f SIZE %.0f TONE %.1f: ringing %.1f dB (< %.0f)", d, z,
                              t, std::isfinite(m.ringingDb) ? m.ringingDb : 0.0, iv::metrics::kRingingGrowthDb);
                check(!m.ringing, msg);
            }
}

// Runs an engine on in, with a parameter moved at sample points (key, sample, value), 48 at a time.
struct Move {
    iv::ParamId id;
    size_t      at;
    float       value;
};
Stereo renderMoves(const std::vector<float>& in, int space, float decay, float size, int attack, const std::vector<Move>& moves)
{
    iv::Engine e;
    e.prepare(48000.0f, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e.setParam(iv::ParamId::Attack, iv::switchToNormalised(attack));
    e.setParam(iv::ParamId::Decay, decay);
    e.setParam(iv::ParamId::Size, size);
    e.setParam(iv::ParamId::Time, 0.1f);
    e.setParam(iv::ParamId::Mix, 1.0f);
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    size_t next = 0;
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        while (next < moves.size() && moves[next].at <= pos) {
            e.setParam(moves[next].id, moves[next].value);
            ++next;
        }
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

// The power left after a narrow notch at f (Q 30), against all of it, in dB: how much isn't the note.
double offNoteDb(const std::vector<float>& x, double f, size_t from, size_t to)
{
    const double w = 2.0 * 3.141592653589793 * f / 48000.0, al = std::sin(w) / 60.0, a0 = 1.0 + al;
    const double b0 = 1.0 / a0, b1 = -2.0 * std::cos(w) / a0, a1 = b1, a2 = (1.0 - al) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, all = 0, rest = 0;
    for (size_t i = 0; i < to && i < x.size(); ++i) {
        const double y = b0 * x[i] + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x[i]; y2 = y1; y1 = y;
        if (i >= from) {
            all += double(x[i]) * x[i];
            rest += y * y;
        }
    }
    return 10.0 * std::log10(rest / (all + 1e-30) + 1e-30);
}

// The strongest frequency within +-2 semitones of f over [from, to) (Goertzel, 0.25 Hz steps).
double peakHz(const std::vector<float>& x, double f, size_t from, size_t to)
{
    double best = 0.0, bestHz = f;
    for (double h = f * 0.89; h <= f * 1.12; h += 0.25) {
        const double c = 2.0 * std::cos(2.0 * 3.141592653589793 * h / 48000.0);
        double s1 = 0, s2 = 0;
        for (size_t i = from; i < to && i < x.size(); ++i) {
            const double s0 = x[i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (p > best) {
            best = p;
            bestHz = h;
        }
    }
    return bestHz;
}

// SIZE and TIME move by crossfade (ADR 0025): a glide in a loop retuned the whole tail by the length
// ratio and swept everything passing through. The tail keeps its pitch through a SIZE move; quick SIZE
// and TIME moves on a held note add nothing but the note; no clicks; an infinite tail holds through
// minutes of SIZE sweeps; and a jump doesn't step the tail's level off its own fade.
void sizeAndTimeCrossfade()
{
    const float sr = 48000.0f;
    {   // a full SIZE sweep on a held tone: no clicks
        const size_t n = size_t(8.0f * sr);
        std::vector<float> in(n), ramp(n);
        for (size_t i = 0; i < n; ++i) {
            in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / sr) + 0.02f * noise();
            const float t = float(i) / sr;
            ramp[i] = t < 1.0f ? 0.0f : (t < 3.0f ? (t - 1.0f) * 0.5f : (t < 5.0f ? 1.0f - (t - 3.0f) * 0.5f : 0.0f));
        }
        const Stereo out = render(in, {0.6f, 0.0f, 0.5f, 1.0f}, sr, false, &ramp);
        const auto m = iv::metrics::compute({out.l, out.r}, sr);
        check(finite(out) && m.clickCount == 0, "SIZE swept 0 -> 1 -> 0 on a held tone: no clicks (" + std::to_string(m.clickCount) + ")");
    }
    {   // MASSIVE's tail keeps its pitch through SIZE 0.3 -> 0.8 (a fast glide left it an octave flat)
        const size_t n = size_t(12.0f * sr);
        std::vector<float> in(n, 0.0f);
        for (size_t i = 0; i < size_t(5.0f * sr); ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * float(i) / sr);
        const Stereo out = renderMoves(in, 2, 0.6f, 0.3f, 0, {{iv::ParamId::Size, size_t(6.0f * sr), 0.8f}});
        const double hz = peakHz(out.l, 440.0, size_t(8.0f * sr), size_t(11.0f * sr));
        const double cents = 1200.0 * std::log2(hz / 440.0);
        check(std::fabs(cents) < 15.0, "MASSIVE's tail keeps its pitch through a SIZE move (" + std::to_string(int(cents)) + " cents)");
    }
    for (int what = 0; what < 2; ++what) { // quick moves on a held note add nothing but the note
        const size_t n = size_t(9.0f * sr);
        std::vector<float> in(n);
        for (size_t i = 0; i < n; ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * float(i) / sr);
        std::vector<Move> moves;
        const iv::ParamId id = what == 0 ? iv::ParamId::Size : iv::ParamId::Time;
        for (int k = 0; k < 5; ++k) moves.push_back({id, size_t((6.0f + 0.3f * float(k)) * sr), k % 2 ? 0.1f : 0.9f});
        const Stereo out = renderMoves(in, 1, 0.5f, 0.1f, 0, moves);
        const double still = offNoteDb(out.l, 440.0, size_t(3.0f * sr), size_t(6.0f * sr));
        const double moving = offNoteDb(out.l, 440.0, size_t(6.0f * sr), size_t(8.5f * sr));
        const auto m = iv::metrics::compute({out.l, out.r}, sr);
        char msg[200];
        std::snprintf(msg, sizeof msg, "HALL, %s flicked on a held note: off the note %.1f dB while moving (%.1f still), no clicks (%d)",
                      what == 0 ? "SIZE" : "TIME", moving, still, m.clickCount);
        // Sliding: -1 dB. A crossfade between two reads of one held sine dips where they're out of phase,
        // so SIZE keeps a little more (-17 dB, back-to-back fades) than TIME's (-22, outside the loop).
        // v1.0.71 (ADR 0047): HALL's floor x2 smears the held note on purpose, still -18.7 -> -12.8 dB, moving (SIZE)
        // -16.9 -> -11.2 (TIME -19.2 -> -11.5); so -10 here, was -15. What moving adds is unchanged.
        check(moving < still + 8.0 && moving < -10.0 && m.clickCount == 0, msg);
    }
    {   // an infinite MASSIVE holds through a minute of SIZE moves (back-to-back crossfades drained 45 dB;
        // equal-power fades grew the lows to a DC offset). Each SIZE has its own level (a bigger room is
        // louder), so: steady between moves, and back at SIZE 0.5 where it began.
        const size_t n = size_t(76.0f * sr);
        std::vector<float> in(n, 0.0f);
        for (size_t i = 0; i < size_t(3.0f * sr); ++i) in[i] = 0.25f * noise();
        std::vector<Move> moves;
        const float sizes[] = {0.2f, 0.9f, 0.4f, 1.0f, 0.0f, 0.7f, 0.3f, 0.8f, 0.5f};
        for (int k = 0; k < 9; ++k) moves.push_back({iv::ParamId::Size, size_t((10.0f + 7.0f * float(k)) * sr), sizes[k]});
        const Stereo out = renderMoves(in, 2, 0.95f, 0.5f, 0, moves);
        double drift = 0.0;
        for (int k = 0; k < 9; ++k) { // 1.5 s after each move against 5.5 s after
            const double t = 10.0 + 7.0 * k;
            drift = std::fmax(drift, std::fabs(rmsDb(out.l, size_t((t + 5.0) * sr), size_t((t + 6.0) * sr))
                                               - rmsDb(out.l, size_t((t + 1.5) * sr), size_t((t + 2.5) * sr))));
        }
        const double back = rmsDb(out.l, size_t(73.0 * sr), size_t(75.0 * sr)) - rmsDb(out.l, size_t(7.0 * sr), size_t(9.0 * sr));
        char msg[200];
        std::snprintf(msg, sizeof msg, "an infinite MASSIVE holds through SIZE moves (drift between moves %.1f dB, back at the start %+.1f dB)", drift, back);
        check(finite(out) && drift < 1.5 && std::fabs(back) < 2.0, msg);
    }
    // A SIZE jump in a tail goes from the old room's fade to the new one's (HALL's decay and every
    // room's level follow SIZE) without dipping under or rising over both (a read further back is louder;
    // small HALLs' diffusion joining in the allpass's own loop passed 14 % mid-fade).
    const float jumps[][2] = {{0.3f, 0.8f}, {0.8f, 0.3f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    for (int space : {1, 2})
        for (const auto& j : jumps) {
            const size_t n = size_t(8.0f * sr);
            std::vector<float> in(n, 0.0f);
            for (size_t i = 0; i < size_t(3.0f * sr); ++i) in[i] = 0.25f * noise();
            const Stereo out = renderMoves(in, space, 0.5f, j[0], 0, {{iv::ParamId::Size, size_t(5.0f * sr), j[1]}});
            auto db = [&](double t) { return rmsDb(out.l, size_t(t * sr), size_t((t + 0.1) * sr)); };
            const double s0 = (db(4.8) - db(3.8)) / 1.0, s1 = (db(6.8) - db(5.8)) / 1.0; // dB per second, each room
            double worst = 0.0;
            for (double t = 4.9; t < 5.8; t += 0.05) {
                const double a = db(4.8) + s0 * (t - 4.8), b = db(5.8) + s1 * (t - 5.8), x = db(t);
                worst = std::fmax(worst, std::fmax(std::fmin(a, b) - x, x - std::fmax(a, b)));
            }
            char msg[160];
            std::snprintf(msg, sizeof msg, "%s: a SIZE jump %.1f->%.1f in a tail stays between the two rooms' fades (%.1f dB out, within 3)",
                          space == 1 ? "HALL" : "MASSIVE", j[0], j[1], worst);
            check(worst < 3.0, msg);
        }
}

void deterministic()
{
    const auto in = burst(48000.0f, 3.0f);
    const Stereo a = render(in, {0.7f, 0.5f, 0.5f}), b = render(in, {0.7f, 0.5f, 0.5f});
    check(a.l == b.l && a.r == b.r, "deterministic: the same input and settings render bit-identically");
}

// SPEC §7 M2 criterion, checked on the Core: T60 within 5 % across rates.
void sampleRates()
{
    double t48 = 0.0;
    bool ok = true;
    std::string detail;
    for (float sr : {48000.0f, 44100.0f, 96000.0f}) {
        std::vector<float> in(size_t(9.0f * sr), 0.0f);
        in[100] = 0.5f;
        const double t = lowBandT60(render(in, {0.4f, 0.5f, 0.5f}, sr), sr);
        if (sr == 48000.0f) t48 = t;
        else ok &= std::isfinite(t) && std::fabs(t / t48 - 1.0) < 0.05;
        char buf[48];
        std::snprintf(buf, sizeof buf, " %.0f Hz %.2f s", double(sr), t);
        detail += buf;
    }
    check(ok, "T60 within 5% at 44.1 / 48 / 96 kHz:" + detail);
}

// True stereo in (ADR 0013): a signal on L alone comes out of both sides,
// and the two outputs are decorrelated (wide).
void stereo()
{
    const float sr = 48000.0f;
    const auto in = burst(sr, 4.0f);
    const Stereo out = render(in, {0.5f, 0.5f, 0.5f}, sr, true);
    const double l = rmsDb(out.l, size_t(0.6f * sr), size_t(2.0f * sr));
    const double r = rmsDb(out.r, size_t(0.6f * sr), size_t(2.0f * sr));
    check(std::fabs(l - r) < 3.0, "L-only input: both outputs carry the tail (L " + std::to_string(int(l)) + " dB, R " +
                                      std::to_string(int(r)) + " dB)");
    const Stereo both = render(in, {0.5f, 0.5f, 0.5f});
    const auto m = iv::metrics::compute({both.l, both.r}, sr);
    char msg[120];
    std::snprintf(msg, sizeof msg, "wide: L/R correlation of the tail %.2f (< 0.5)", m.stereoCorrelation);
    check(std::isfinite(m.stereoCorrelation) && m.stereoCorrelation < 0.5, msg);
}

// The floor governor's weight across DECAY (v1.0.56, ADR 0008 amended): PLATE and MASSIVE ease it in
// from 0.70 (t^3), HALL as before; 0 at and below 0.70 and 1 from 0.85 in every SPACE (so short settings
// and endless are as they were), never falling as DECAY rises.
void floorWeightRamp()
{
    bool ends = true, hall = true, rising = true;
    for (int space = 0; space < 3; ++space) {
        float was = 0.0f;
        for (int i = 0; i <= 100; ++i) {
            const float d = 0.01f * float(i), w = iv::map::floorWeight(d, space);
            if (d <= 0.70f) ends &= w == 0.0f;
            if (d >= 0.85f) ends &= w == 1.0f;
            if (space == 1) hall &= w == iv::map::governorWeight(d);
            rising &= w >= was;
            was = w;
        }
    }
    check(ends && hall && rising, "floor governor's weight: 0 to DECAY 0.70, 1 from 0.85, rising; HALL's as governorWeight");
}

} // namespace

int main()
{
    stabilityGrid();
    t60FollowsDecay();
    floorWeightRamp();
    notMetallic();
    sizeAndTimeCrossfade();
    deterministic();
    sampleRates();
    stereo();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
