// M6: the loop's character (SPEC §4.6, §4.7, §7 M6; ADR 0005, 0008, 0012). ERA as one wear amount (v1.0.42,
// the owner's pick C on renders/era_rethink): the Neve base in the loop, the wear on the wet. The wear doesn't
// darken the loop (TONE's job) nor move the pitch (DRIFT's); loudness on music stays within ±1 dB across the
// knob; it is on the wet only; a finite tail still reaches true silence and silence stays silent; its noise
// follows the input; infinite tails hold across the knob in every SPACE; turning ERA doesn't click (1 ms
// frames); DRIFT's dead zone is exact and both sides move the tail without clicks; the saturation doesn't
// alias badly when hit hard. v1.0.49 (the owner's picks on renders/wear_grit, wear_fizz, wear_saturation): the
// grit (the grain; the BBD's clock too until v1.0.64) is silent below 0.65 on the knob (below noon since v1.0.69, ADR 0044).
// v1.0.60 (ADR 0036, the owner's pick F on renders/dip_motion): the radio's band left WEAR for MOTION's right side.
// v1.0.64 (ADR 0038, the owner's pick C on renders/wear_no_bbd): the BBD's clock left WEAR, so WEAR is tape,
// breath and grain; it never narrows the tone, and it no longer folds the top back.

#include "DecayKnobFor.h"
#include "Fft.h"
#include "Metrics.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "dsp/EraTexture.h"
#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <complex>
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

uint32_t gSeed = 31;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}

struct Stereo {
    std::vector<float> l, r;
};

struct Setting {
    int   space = 1;
    float era = 0.0f, drift = 0.5f, decay = 0.5f, size = 0.5f, mix = 1.0f, tone = 0.5f;
};

Stereo render(const std::vector<float>& in, const Setting& s, float sr = kSr)
{
    iv::Engine e;
    e.prepare(sr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Era, s.era);
    e.setParam(iv::ParamId::Drift, s.drift);
    e.setParam(iv::ParamId::Decay, decayKnobFor(s.decay));
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Mix, s.mix);
    e.setParam(iv::ParamId::Tone, s.tone);
    e.setParam(iv::ParamId::Time, 0.0f);
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

double rmsDb(const std::vector<float>& x, double from, double to)
{
    const size_t a = size_t(from * kSr), b = size_t(to * kSr);
    double sum = 0.0;
    for (size_t i = a; i < b && i < x.size(); ++i) sum += double(x[i]) * x[i];
    return 10.0 * std::log10(sum / double(b - a) + 1e-30);
}

bool finite(const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}

// Low-band (< 500 Hz) and high-band (> 4 kHz) T60s of a tail, via one-pole
// filters and the metrics' Schroeder T60.
void bandT60s(const Stereo& out, double& low, double& high)
{
    std::vector<float> lo(out.l.size()), hi(out.l.size());
    const float cl = 1.0f - std::exp(-2.0f * 3.14159265f * 500.0f / kSr);
    const float ch = std::exp(-2.0f * 3.14159265f * 4000.0f / kSr);
    float a = 0, b = 0, hx = 0, hy = 0, hx2 = 0, hy2 = 0;
    for (size_t i = 0; i < lo.size(); ++i) {
        const float x = 0.5f * (out.l[i] + out.r[i]);
        a += cl * (x - a);
        b += cl * (a - b);
        lo[i] = b;
        hy = ch * (hy + x - hx); // two one-pole high-passes
        hx = x;
        hy2 = ch * (hy2 + hy - hx2);
        hx2 = hy;
        hi[i] = hy2;
    }
    low = iv::metrics::compute({lo}, kSr).t60S;
    high = iv::metrics::compute({hi}, kSr).t60S;
}

// The wear doesn't darken the loop (v1.0.42: TONE owns the highs): the tail's highs' T60 over its lows' stays
// the Neve base's across the knob, in every SPACE.
void eraDoesntDarken()
{
    std::vector<float> in(size_t(12.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.2f * kSr); ++i) in[i] = 0.3f * noise();
    for (int space : {0, 1}) { // (MASSIVE's tail outlasts the render)
        double base = 0.0, worst = 0.0;
        std::string detail;
        for (float era : {0.0f, 0.3f, 0.7f, 0.8f, 1.0f}) {
            Setting s;
            s.space = space;
            s.era = era;
            double lo, hi;
            bandT60s(render(in, s), lo, hi);
            const double ratio = hi / lo;
            if (era == 0.0f) base = ratio;
            else worst = std::fmin(worst, ratio - base);
            char buf[64];
            std::snprintf(buf, sizeof buf, " %.1f: %.2f", era, ratio);
            detail += buf;
        }
        check(worst > -0.05, "SPACE " + std::to_string(space) + ": the wear doesn't darken the tail against the Neve base (high/low T60, ERA" +
                                 detail + ")");
    }
}

// SPEC §7 M6 (v1.0.19): ERA's loudness on music stays within ±1 dB across the
// knob, measured on the dub skank (test_audio/stimulus/04_skank.wav, from
// tools/make_stimulus.py) with BS.1770's K-weighting at 48 kHz, over the whole
// render. Until v1.0.19 this measured noise bursts' tails, where ERA's darkening
// costs level, and the make-up set from it made music 4-6 LU louder at the
// Midiverb end (owner, M6). Synthetic chords didn't track music either way
// (sparse ones read 3.5 dB down, sustained sines +2.3 up in MASSIVE).
double kWeightedDb(const Stereo& s)
{
    double sum = 0.0;
    for (const auto* ch : {&s.l, &s.r}) {
        double a1 = 0, a2 = 0, b1 = 0, b2 = 0; // two direct-form-II states
        for (float x : *ch) {
            const double w = x + 1.69065929318241 * a1 - 0.73248077421585 * a2; // the head's shelf
            const double y = 1.53512485958697 * w - 2.69169618940638 * a1 + 1.19839281085285 * a2;
            a2 = a1; a1 = w;
            const double v = y + 1.99004745483398 * b1 - 0.99007225036621 * b2; // the RLB high-pass
            const double k = v - 2.0 * b1 + b2;
            b2 = b1; b1 = v;
            sum += k * k;
        }
    }
    return 10.0 * std::log10(sum / double(2 * s.l.size()) + 1e-30) - 0.691;
}

void eraLoudness()
{
    iv::wav::Audio a;
    std::string error;
    bool read = false;
    for (const char* prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (!read) read = iv::wav::read(std::string(prefix) + "04_skank.wav", a, error);
    if (!read) { check(false, "ERA loudness: test_audio/stimulus/04_skank.wav (python3 tools/make_stimulus.py)"); return; }
    std::vector<float> in(a.channels[0]);
    in.resize(in.size() + size_t(6.0f * kSr), 0.0f);
    struct Space { int space; float decay, size; const char* name; };
    for (const Space& sp : {Space{1, 0.6f, 0.6f, "HALL"}, Space{2, 0.65f, 0.7f, "MASSIVE"}}) {
        double lo = 1e9, hi = -1e9;
        std::string detail;
        for (float era : {0.0f, 0.1f, 0.3f, 0.5f, 0.65f, 0.7f, 0.8f, 0.9f, 1.0f}) {
            Setting s;
            s.space = sp.space;
            s.decay = sp.decay;
            s.size = sp.size;
            s.era = era;
            const double db = kWeightedDb(render(in, s));
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
            char buf[48];
            std::snprintf(buf, sizeof buf, " %.2f: %.1f", era, db);
            detail += buf;
        }
        check(hi - lo <= 2.0, std::string(sp.name) + ": ERA's loudness on music (LUFS) within ±1 dB across the knob:" + detail);
    }
}

// The wear is on the wet only: MIX 0 is still a bit-identical passthrough.
void eraOnWetOnly()
{
    std::vector<float> in(size_t(2.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin(2.0f * 3.14159265f * 1000.0f * float(i) / kSr);
    for (float era : {0.5f, 1.0f}) {
        Setting dry;
        dry.era = era;
        dry.mix = 0.0f;
        const Stereo out = render(in, dry);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same &= out.l[i] == in[i];
        check(same, "ERA " + std::to_string(era).substr(0, 3) + " is on the wet only: MIX 0 is a bit-identical passthrough");
    }
}

// A finite tail at full wear reaches true silence (the grain truncates toward zero, its step has a floor, the
// breath's noise follows the level: no stuck buzz or hiss), and silence in stays silence out.
void tailReachesSilence()
{
    std::vector<float> in(size_t(60.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.5f * kSr); ++i) in[i] = 0.3f * noise();
    for (int space : {0, 1}) {
        Setting s;
        s.space = space;
        s.era = 1.0f;
        s.decay = 0.5f;
        const Stereo out = render(in, s);
        const double end = rmsDb(out.l, 55.0, 60.0);
        check(finite(out) && end < -110.0, "SPACE " + std::to_string(space) + ", full wear, DECAY 0.5: the tail reaches silence (" +
                                               std::to_string(int(end)) + " dBFS at 55-60 s)");
    }
    const std::vector<float> zeros(size_t(10.0f * kSr), 0.0f);
    for (int space : {0, 1, 2}) {
        Setting s;
        s.space = space;
        s.era = 1.0f;
        s.decay = 1.0f;
        const Stereo out = render(zeros, s);
        bool silent = true;
        for (size_t i = 0; i < out.l.size(); ++i) silent &= out.l[i] == 0.0f && out.r[i] == 0.0f;
        check(silent, "SPACE " + std::to_string(space) + ", full wear, DECAY infinite: silence in is silence out");
    }
}

// ADR 0005: noise follows the input: none once the input is silent (the tape's hiss, the breath's noise,
// the crackle).
void hissFollowsInput()
{
    std::vector<float> in(size_t(120.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(1.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (float era : {0.1f, 0.5f, 1.0f}) {
        Setting s;
        s.era = era;
        s.decay = 0.3f;
        const Stereo out = render(in, s);
        const double end = rmsDb(out.l, 115.0, 120.0);
 check(finite(out) && end < -90.0, "ERA " + std::to_string(era).substr(0, 3) +
                                              ": the wear's noise is gone in silence (" + std::to_string(int(end)) + " dBFS after 2 min)");
    }
}

// ADR 0008: an infinite tail holds across the knob in every SPACE (10 min, within 3 dB, no growth, not
// silent). The level is judged over 5 s (until v1.0.60 the band's wandering dip came and went on WEAR's wet).
void infiniteAcrossTheKnob()
{
    std::vector<float> in(size_t(620.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(20.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (int space : {0, 1, 2})
        for (float era : {0.0f, 0.1f, 0.5f, 0.9f, 1.0f}) {
            Setting s;
            s.space = space;
            s.era = era;
            s.decay = 1.0f;
            const Stereo out = render(in, s);
            const double win = 5.0;
            const double first = rmsDb(out.l, 60.0, 70.0);
            double lo = first, hi = first;
            for (double t = 60.0; t + win < 620.0; t += 30.0) {
                const double db = rmsDb(out.l, t, t + win);
                lo = std::fmin(lo, db);
                hi = std::fmax(hi, db);
            }
            char msg[160];
            std::snprintf(msg, sizeof msg, "SPACE %d: infinite holds 10 min at ERA %.1f: %.1f .. %.1f dBFS", space, era, lo, hi);
            check(finite(out) && hi - lo < 3.0 && hi <= first + 1.0 && lo > -60.0, msg);
        }
}

// The wear moves no pitch (DRIFT's job): a held tone's frequency through the tail, second by second, wanders
// no more at full wear than with the Neve alone (DRIFT at noon: only the tanks' own modulation floor).
double peakHz(const std::vector<float>& x, size_t from, size_t n, double lo, double hi)
{
    auto mag = [&](double f) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * 3.141592653589793 * f / kSr;
        for (size_t i = 0; i < n; ++i) {
            const double h = 0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * double(i) / double(n));
            re += h * x[from + i] * std::cos(w * double(i));
            im -= h * x[from + i] * std::sin(w * double(i));
        }
        return re * re + im * im;
    };
    double best = lo, bestM = -1.0;
    for (double f = lo; f <= hi; f += 0.5)
        if (const double m = mag(f); m > bestM) { bestM = m; best = f; }
    const double a = best - 0.5, b = best + 0.5;
    for (double f = a; f <= b; f += 0.01)
        if (const double m = mag(f); m > bestM) { bestM = m; best = f; }
    return best;
}

void eraMovesNoPitch()
{
    std::vector<float> in(size_t(9.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * float(i) / kSr);
    double spread[2] = {}, mean[2] = {};
    for (int k = 0; k < 2; ++k) {
        Setting s;
        s.era = k == 0 ? 0.0f : 1.0f;
        const Stereo out = render(in, s);
        double lo = 1e9, hi = -1e9, sum = 0.0;
        for (int w = 0; w < 6; ++w) {
            const double hz = peakHz(out.l, size_t((2.5 + w) * kSr), size_t(kSr), 430.0, 450.0);
            lo = std::fmin(lo, hz);
            hi = std::fmax(hi, hz);
            sum += hz;
        }
        spread[k] = 1200.0 * std::log2(hi / lo);
        mean[k] = sum / 6.0;
    }
    const double offset = 1200.0 * std::log2(mean[1] / mean[0]);
    char msg[200];
    std::snprintf(msg, sizeof msg, "the wear moves no pitch: a held 440 Hz wanders %.2f cents a second at full wear, %.2f with the Neve alone; "
                  "its mean %.2f cents off", spread[1], spread[0], offset);
    check(spread[1] <= spread[0] + 0.5 && std::fabs(offset) < 0.5, msg);
}

// DRIFT: the dead zone is exact (noon ± 3 % renders bit-identically), and
// both sides move the tail without clicks.
void drift()
{
    std::vector<float> in(size_t(10.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 330.0f * float(i) / kSr) + 0.01f * noise();
    Setting noon, edge;
    edge.drift = iv::voicing::kDriftDeadLo + 0.001f;
    const Stereo a = render(in, noon), b = render(in, edge);
    check(a.l == b.l && a.r == b.r, "DRIFT dead zone: noon and its edge render bit-identically");
    for (float d : {0.0f, 1.0f}) {
        Setting s;
        s.drift = d;
        const Stereo out = render(in, s);
        const auto m = iv::metrics::compute({out.l, out.r}, kSr);
        bool moved = false;
        for (size_t i = 0; i < out.l.size() && !moved; ++i) moved = out.l[i] != a.l[i];
        check(finite(out) && moved && m.clickCount == 0,
              std::string("DRIFT fully ") + (d == 0.0f ? "left (tape)" : "right (drift)") + " moves the tail, no clicks (" +
                  std::to_string(m.clickCount) + ")");
    }
}

// Turning ERA doesn't click: up and down the whole knob, and quickly through its start (where every stage
// comes in) and back, in HALL and PLATE, on a held chord. Judged two ways: the metrics' click count (a
// singular glitch above the local second difference), and as renders/era_rethink judged it, the loudest
// 1 ms frame above 5 kHz against the median of the half second around it, which must stay within 3 dB of
// the same chord's at a still ERA (the wear's own grit reads there too).
double clickScanDb(const Stereo& out, double from, double to, double* levelDb = nullptr)
{
    // Above 5 kHz: two 2nd-order Butterworth high-passes (RBJ) in a row.
    const double w = 2.0 * 3.141592653589793 * 5000.0 / kSr, al = std::sin(w) / (2.0 * 0.7071067811865476), c = std::cos(w);
    const double b0 = (1 + c) / 2 / (1 + al), b1 = -(1 + c) / (1 + al), a1 = -2 * c / (1 + al), a2 = (1 - al) / (1 + al);
    std::vector<double> h(out.l.size());
    double x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {};
    for (size_t i = 0; i < h.size(); ++i) {
        double x = 0.5 * (double(out.l[i]) + out.r[i]);
        for (int k = 0; k < 2; ++k) {
            const double y = b0 * x + b1 * x1[k] + b0 * x2[k] - a1 * y1[k] - a2 * y2[k];
            x2[k] = x1[k]; x1[k] = x; y2[k] = y1[k]; y1[k] = y;
            x = y;
        }
        h[i] = x;
    }
    const size_t f = size_t(0.001f * kSr), frames = h.size() / f, half = 250;
    std::vector<double> e(frames);
    for (size_t k = 0; k < frames; ++k) {
        double sum = 0.0;
        for (size_t i = k * f; i < (k + 1) * f; ++i) sum += h[i] * h[i];
        e[k] = sum / double(f) + 1e-20;
    }
    if (levelDb) { // the median 1 ms frame's level over [from, to): the steady fizz above 5 kHz
        std::vector<double> span(e.begin() + long(from * 1000.0), e.begin() + long(std::min(double(frames), to * 1000.0)));
        std::nth_element(span.begin(), span.begin() + long(span.size() / 2), span.end());
        *levelDb = 10.0 * std::log10(span[span.size() / 2]);
    }
    double best = -99.0;
    for (size_t k = size_t(from * 1000.0); k < size_t(to * 1000.0) && k + half < frames; ++k) {
        std::vector<double> around(e.begin() + long(k - half), e.begin() + long(k + half));
        std::nth_element(around.begin(), around.begin() + long(half), around.end());
        best = std::fmax(best, 10.0 * std::log10(e[k] / around[half]));
    }
    return best;
}

void eraSweepClickFree()
{
    std::vector<float> in(size_t(7.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / kSr) + 0.1f * std::sin(2.0f * 3.14159265f * 331.0f * float(i) / kSr);
    for (int space : {1, 0}) {
        double still = -99.0, hf80 = 0.0;
        for (int mode : {-1, -2, -3, -4, 0, 1, 2, 3}) { // still at 0, 0.5, 1, 0.8; up 0 -> 1 over 5 s; down 1 -> 0; 0 -> 0.15 -> 0 in a quarter
                                                // second each way; 3: 0.4 -> 0.65 -> 0.4 likewise (through the grit's start at noon, v1.0.69; 0.55 -> 0.8 for 0.65)
            iv::Engine e;
            e.prepare(kSr, 48);
            e.setParam(iv::ParamId::Space, iv::switchToNormalised(space));
            e.setParam(iv::ParamId::Mix, 1.0f);
            const float rest = mode == -2 ? 0.5f : (mode == -3 || mode == 1) ? 1.0f : mode == -4 ? 0.8f : mode == 3 ? 0.4f : 0.0f;
            e.setParam(iv::ParamId::Era, rest);
            e.reset();
            Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                const float sec = float(pos) / kSr;
                const float t = std::fmin(1.0f, std::fmax(0.0f, (sec - 1.0f) / 5.0f));
                float era = mode == 0 ? t : mode == 1 ? 1.0f - t : rest;
                if (mode == 2) {
                    const float u = sec - 2.0f;
                    era = u < 0.0f ? 0.0f : u < 0.25f ? 0.6f * u : u < 0.5f ? 0.15f - 0.6f * (u - 0.25f) : 0.0f;
                }
                if (mode == 3) {
                    const float u = sec - 2.0f;
                    era = u < 0.0f ? 0.4f : u < 0.25f ? 0.4f + u : u < 0.5f ? 0.65f - (u - 0.25f) : 0.4f;
                }
                e.setParam(iv::ParamId::Era, era);
                const int n = int(std::min<size_t>(48, in.size() - pos));
                e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
            }
            const auto m = iv::metrics::compute({out.l, out.r}, kSr);
            double hf = 0.0;
            const double scan = clickScanDb(out, 0.8, 6.7, &hf);
            if (mode == -4) hf80 = hf;
            if (mode < 0) {
                still = std::fmax(still, scan);
                continue;
            }
            // Quickly through the grit's start (mode 3): the grit comes in and goes within the half second the scan
            // judges against, so its frames stand over their median by the grit's own rise over the wear without it
            // (the steady fizz above 5 kHz at a still 0.8 over the sweep's still 0.4), not by a click.
            if (mode == 3) {
                const double rise = hf80 - hf; // hf: this sweep's own median, at rest at 0.4 for all but half a second
                std::printf("       the grit's rise above 5 kHz, 0.4 -> 0.8 still: %+.1f dB\n", rise);
                still = std::fmax(still, rise);
            }
            char msg[200];
            std::snprintf(msg, sizeof msg, "SPACE %d: ERA turned %s, no clicks (%ld; loudest 1 ms frame above 5 kHz %+.1f dB, still ERA %+.1f)",
                          space, mode == 0 ? "up" : mode == 1 ? "down" : mode == 2 ? "quickly through its start and back"
                                                                          : "quickly through the grit's start (noon) and back",
                          m.clickCount, scan, still);
            check(finite(out) && m.clickCount == 0 && scan < still + 3.0, msg);
        }
    }
}

// SPEC §7 M6: the saturation hit hard doesn't alias badly. A loud 7 kHz
// tone's 3rd/5th harmonics fold to 27 → 21 kHz and 35 → 13 kHz: check the
// band below 5 kHz, where nothing harmonic belongs. The wear's tape (its bias:
// even harmonics as tape's) puts real difference tones there too, so
// the same tone is also rendered at 96 kHz, where little folds: aliasing is
// what 48 kHz adds over that (or the band simply stays 40 dB down).
// Averaged over five tones 6.6-7.4 kHz (v1.0.36): the tank's early taps comb a single tone and its
// fold-back, so one tone's figure is luck (7 kHz alone: -37 dB with six taps, -31 with two; the five
// average -33.6 and -33.5).
double lowBandDb(float sr, float hz)
{
    std::vector<float> in(size_t(3.0f * sr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.9f * std::sin(2.0f * 3.14159265f * hz * float(i) / sr);
    Setting s;
    s.era = 0.75f; // the wear's tape well driven
    s.decay = 0.0f;
    const Stereo out = render(in, s, sr);
    if (!finite(out)) return 999.0;
    const size_t n = sr > 50000.0f ? 65536 : 32768, start = size_t(2.0f * sr) - (sr > 50000.0f ? 65536 - 32768 : 0);
    const auto spec = iv::fft::magnitudeSpectrum(out.l.data() + start, n, n);
    double lowBand = 0.0, total = 0.0;
    for (size_t k = 1; k < spec.size(); ++k) {
        const double p = double(spec[k]) * spec[k];
        const float f = float(k) * sr / float(n);
        if (f > 24000.0f) continue; // the 96 kHz render: compare the same audio band
        total += p;
        if (f < 5000.0f) lowBand += p;
    }
    return lowBand / (total + 1e-30);
}

double lowBandDb(float sr)
{
    double share = 0.0;
    for (float hz : {6600.0f, 6800.0f, 7000.0f, 7200.0f, 7400.0f}) share += lowBandDb(sr, hz) / 5.0;
    return 10.0 * std::log10(share + 1e-30);
}

void notAliasing()
{
    const double at48 = lowBandDb(kSr), at96 = lowBandDb(96000.0f);
    // v1.0.35 (ADR 0026): the HALL tank runs at 24 kHz, so in its loop the 3rd harmonic (~21 kHz) folds to
    // ~3 kHz: about -34 dB here (was under -40). Accepted for the CPU (classic digital reverbs fold the
    // same way); the limit for it is -32.
    // v1.0.49: the grit now starts at 0.65, so at 0.75 it is light (then with the clock at ~31 kHz, a third in; the
    // clock left in v1.0.64) and no longer
    // covers the tape's own folding (its 7th harmonic, 49 kHz, lands at 1 kHz), which reads 4-5 dB over the 96 kHz
    // render. Until v1.0.48 the grit's images (by design, at both rates) passed the second clause; with the grit
    // out, v1.0.48's tape read 9 dB over at 0.75 (5.7 at 0.64), the softer tape 4.1 at 0.64. Whole, the band below
    // 5 kHz fell from -9 dB re total (v1.0.48) to -18: the third clause holds that.
    // v1.0.65 (ADR 0040, the owner's pick D on renders/more_tape, "more saturation"): the tape driven 5 dB harder folds
    // more: -20.2 dB here, 6.3 over the 96 kHz render (v1.0.64: -23.3, 2.6 over). The owner picked it on the page's
    // bright stabs, where it shows; the third clause allows 7 dB (was 6).
    // With v1.0.63 (ADR 0039, the onset centred) in the same build: -19.4 dB here, much as v1.0.65 alone, but the
    // 96 kHz render reads 2 dB cleaner (-28.5), so the gap is 9.1 dB. The 48 kHz fold is what moved least; the
    // third clause allowed 10 dB.
    // v1.0.66 (ADR 0041, the owner's pick B on renders/tape_fold): the drive back to 11.25 dB: -21.9 dB here, -27.8 at
    // 96 kHz, a 5.9 dB gap; the third clause allows 7 dB again.
    // v1.0.69 (ADR 0044, the owner's pick D on renders/wear_taper): WEAR's intensities on 1 - (1 - v)^2.2, so 0.75 drives
    // the tape 10.7 dB (was 8.4), near full, and the grit is half in (from noon): -18.8 dB here, -26.2 at 96 kHz, a 7.4 dB
    // gap, as on the 3 o'clock cards the owner picked; the third clause allows 8 dB.
    // v1.0.70 (ADR 0045, the owner's pick C on renders/wear_drive): the drive back to v1.0.65's 16.25 dB on that taper, so
    // 0.75 drives the tape 15.5 dB (v1.0.65 there: 12.2): -16.7 dB here, -25.1 at 96 kHz, an 8.4 dB gap, under v1.0.65's
    // own with v1.0.63 (9.1); the third clause allows 10 dB again, as it did then. The band below 5 kHz is still under -15.
    char msg[240];
    std::snprintf(msg, sizeof msg, "hot 7 kHz tone at ERA 0.75: energy below 5 kHz %.1f dB re total at 48 kHz, %.1f at 96 kHz (< -32 with the "
                                   "24 kHz tank, or aliasing adds < 2 dB, or the band under -15 with aliasing adding < 10 dB, v1.0.70)", at48, at96);
    check(at48 < -32.0 || at48 - at96 < 2.0 || (at48 < -15.0 && at48 - at96 < 10.0), msg);
}

// v1.0.49 (renders/wear_grit D): the grit (the grain; the BBD's clock too until v1.0.64) is silent below kWearGritStart
// (noon since v1.0.69, ADR 0044; 0.65 before) on the
// knob, then comes in. The wear's chain run twice on the same wet (a chord and a little noise), once as the knob has
// it and once with the grain out: the difference is the grit, its level against the wet's. Below its start the two
// are the same to the bit; past it the grit is there and grows to full.
double gritDb(float v)
{
    const iv::map::EraWear w = iv::map::eraMix(v);
    iv::dsp::TexMix noGrit = w.tex;
    noGrit.grainW = 0.0f;
    iv::dsp::EraTexture a, b;
    a.reset();
    b.reset();
    gSeed = 31;
    double grit = 0.0, wet = 0.0;
    const int n = 48, blocks = int(4.0f * kSr) / n;
    for (int k = 0; k < blocks; ++k) {
        a.set(w.tex, kSr, n);
        b.set(noGrit, kSr, n);
        for (int i = 0; i < n; ++i) {
            const float t = float(k * n + i) / kSr;
            const float x = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * t) + 0.1f * std::sin(2.0f * 3.14159265f * 331.0f * t) +
                            0.02f * noise();
            float al = x, ar = x, bl = x, br = x;
            a.process(al, ar);
            b.process(bl, br);
            if (k * n + i < int(kSr)) continue; // the first second settles
            grit += double(al - bl) * (al - bl);
            wet += double(bl) * bl;
        }
    }
    return grit == 0.0 ? -999.0 : 10.0 * std::log10(grit / (wet + 1e-30));
}

void gritFromItsStart()
{
    std::string detail;
    bool silentBelow = true, thereAbove = true;
    double last = -999.0;
    bool grows = true;
    for (float v : {0.05f, 0.3f, 0.45f, 0.49f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f}) {
        const double db = gritDb(v);
        char buf[40];
        std::snprintf(buf, sizeof buf, " %.2f: %.0f", v, db < -900.0 ? -999.0 : db);
        detail += buf;
        if (v <= iv::voicing::kWearGritStart) silentBelow &= db < -900.0;
        else {
            thereAbove &= db > -70.0; // v1.0.64: the grain alone at 0.70 reads -62 (over -60 with the clock)
            grows &= db >= last - 1.0;
            last = db;
        }
    }
    check(silentBelow && thereAbove && grows,
          "WEAR's grit (the grain) silent to noon, then in and growing (dB re the wet:" + detail + ")");
    const iv::map::EraWear full = iv::map::eraMix(1.0f), start = iv::map::eraMix(iv::voicing::kWearGritStart);
    check(full.tex.grainW == 1.0f && start.tex.grainW == 0.0f, "WEAR 1: the grit whole (the grain's blend 1); at its start (noon) none");
}

// v1.0.60: WEAR carries none of the radio's band at any setting (it is MOTION's right side's, Mappings.h radioBand),
// and with MOTION at noon its wet keeps the lows a band-pass would take: a 100 Hz tone through WEAR 1 within 1.5 dB
// of WEAR 0 (v1.0.59, the band on WEAR: -4.9 dB, after its level hold).
void noBandOnWear()
{
    bool none = true;
    for (float v = 0.0f; v <= 1.0f; v += 0.01f) none = none && iv::map::eraMix(v).tex.bandW == 0.0f;
    std::vector<float> in(size_t(4.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(2.0f * kSr); ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 100.0f * float(i) / kSr);
    double db[2];
    for (int k = 0; k < 2; ++k) {
        Setting s;
        s.space = 1;
        s.era = k == 0 ? 0.0f : 1.0f;
        const Stereo out = render(in, s);
        double sum = 0.0;
        for (size_t i = size_t(1.0f * kSr); i < size_t(2.0f * kSr); ++i) sum += double(out.l[i]) * out.l[i];
        db[k] = 10.0 * std::log10(sum / kSr + 1e-30);
    }
    char msg[160];
    std::snprintf(msg, sizeof msg, "WEAR carries no radio band (blend 0 across the knob); a 100 Hz tone at WEAR 1 %+.1f dB against "
                  "WEAR 0 (within 2)", db[1] - db[0]);
    // (+1.2 until v1.0.70; +1.9 with the still lows in step, ADR 0047: not a band, which would cut this tone.)
    check(none && std::fabs(db[1] - db[0]) <= 2.0, msg);
}

// v1.0.64 (ADR 0038, the owner's pick C on renders/wear_no_bbd): no BBD clock on WEAR. Until v1.0.63 its sample and
// hold at 28 kHz imaged a 1 kHz tone at 27 and 29 kHz, heard folded about 48 kHz at 21 and 19 kHz. Now WEAR 1's whole
// chain (tape, breath, grain) on that tone leaves those bands at the grain's and the breath's noise floor: measured
// -54.9 dB re the tone (with the clock, v1.0.63: -27.6).
void noClockImages()
{
    const iv::dsp::TexMix m = iv::map::eraMix(1.0f).tex;
    iv::dsp::EraTexture t;
    t.reset();
    const size_t len = size_t(2.0f * kSr), n = 32768;
    std::vector<float> out(len);
    for (size_t pos = 0; pos < len; pos += 48) {
        t.set(m, kSr, 48);
        for (size_t i = pos; i < pos + 48; ++i) {
            float l = 0.3f * std::sin(2.0f * 3.14159265f * 1000.0f * float(i) / kSr), r = l;
            t.process(l, r);
            out[i] = l;
        }
    }
    const auto spec = iv::fft::magnitudeSpectrum(out.data() + (len - n), n, n);
    auto band = [&](float hz) {
        double p = 0.0;
        for (size_t k = 1; k < spec.size(); ++k)
            if (std::fabs(float(k) * kSr / float(n) - hz) < 150.0f) p += double(spec[k]) * spec[k];
        return 10.0 * std::log10(p + 1e-30);
    };
    const double images = std::fmax(band(19000.0f), band(21000.0f)) - band(1000.0f);
    char msg[200];
    std::snprintf(msg, sizeof msg, "WEAR 1: no BBD clock images (a 1 kHz tone's 19/21 kHz bands %.1f dB re the tone; under -45; -27.6 with the clock)", images);
    check(images < -45.0, msg);
}

} // namespace

int main()
{
    eraDoesntDarken();
    eraLoudness();
    eraOnWetOnly();
    tailReachesSilence();
    hissFollowsInput();
    infiniteAcrossTheKnob();
    eraMovesNoPitch();
    drift();
    eraSweepClickFree();
    notAliasing();
    gritFromItsStart();
    noClockImages();
    noBandOnWear();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
