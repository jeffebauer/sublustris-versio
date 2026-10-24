// M6: the loop's character (SPEC §4.6, §4.7, §7 M6; ADR 0004, 0005, 0008, 0012, 0021). ERA's five eras
// (Neve base, Midiverb, bucket-brigade, cassette, AM radio): the Midiverb and cassette darken the tail
// against the Neve base; loudness on music stays within ±1 dB across the knob; the BBD and radio are on
// the wet only; the Midiverb box lets a finite tail reach true silence (no stuck buzz); the BBD's,
// cassette's and radio's noise follows the input (none in silence); infinite tails hold at every era;
// turning ERA through the eras doesn't click; DRIFT's dead zone is exact and both sides move the tail
// without clicks; the saturation doesn't alias badly when hit hard.

#include "Fft.h"
#include "Metrics.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

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
    e.setParam(iv::ParamId::Decay, s.decay);
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

// The Midiverb and the cassette darken the tail against the Neve base: the highs' T60 over the lows' falls.
void eraDarkens()
{
    std::vector<float> in(size_t(12.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.2f * kSr); ++i) in[i] = 0.3f * noise(); // a burst: one click is too quiet for the Midiverb's 13 bits
    double base = 0.0;
    bool falling = true;
    std::string detail;
    for (float era : {0.1f, 0.3f, 0.7f}) { // Neve, Midiverb, cassette
        Setting s;
        s.era = era;
        double lo, hi;
        bandT60s(render(in, s), lo, hi);
        const double ratio = hi / lo;
        if (era < 0.2f) base = ratio;
        else falling &= ratio < base - 0.02;
        char buf[64];
        std::snprintf(buf, sizeof buf, " ERA %.1f: %.2f", era, ratio);
        detail += buf;
    }
    check(falling, "the Midiverb and cassette darken the tail against the Neve base (high/low T60:" + detail + ")");
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
        for (float era : {0.0f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f}) { // Neve, Midiverb, BBD, cassette, radio
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

// The BBD and the radio are on the wet only: MIX 0 there is still a bit-identical passthrough.
void erasOnWetOnly()
{
    std::vector<float> in(size_t(2.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin(2.0f * 3.14159265f * 1000.0f * float(i) / kSr);
    for (float era : {0.5f, 0.9f}) {
        Setting dry;
        dry.era = era;
        dry.mix = 0.0f;
        const Stereo out = render(in, dry);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same &= out.l[i] == in[i];
        check(same, std::string("ERA ") + (era < 0.6f ? "0.5 (BBD)" : "0.9 (radio)") + " is on the wet only: MIX 0 is a bit-identical passthrough");
    }
}

// ADR 0004: the Midiverb lets a finite tail reach true silence (the box truncates toward zero: no stuck buzz).
void midiverbReachesSilence()
{
    std::vector<float> in(size_t(60.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.5f * kSr); ++i) in[i] = 0.3f * noise();
    Setting s;
    s.era = 0.3f; // the Midiverb
    s.decay = 0.5f;
    const Stereo out = render(in, s);
    const double end = rmsDb(out.l, 55.0, 60.0);
    check(finite(out) && end < -110.0, "Midiverb, DECAY 0.5: the tail reaches silence, no stuck buzz (" +
                                           std::to_string(int(end)) + " dBFS at 55-60 s)");
}

// ADR 0005: noise follows the input: none once the input is silent (the cassette's hiss, the BBD's
// compander noise, the radio's hiss and static).
void hissFollowsInput()
{
    std::vector<float> in(size_t(120.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(1.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (float era : {0.5f, 0.7f, 0.9f}) {
        Setting s;
        s.era = era;
        s.decay = 0.3f;
        const Stereo out = render(in, s);
        const double end = rmsDb(out.l, 115.0, 120.0);
        check(finite(out) && end < -90.0, std::string(era < 0.6f ? "BBD" : era < 0.8f ? "cassette" : "radio") +
                                              " noise is gone in silence (" + std::to_string(int(end)) + " dBFS after 2 min)");
    }
}

// ADR 0008: an infinite tail holds at every era (10 min, within 3 dB, no growth, not silent). The radio's
// station fades are on the wet after the loop and come and go by design (the deepest beyond what its AGC
// gives back), so there the level is judged over 25 s stretches, which average the fades out.
void infiniteAtEveryEra()
{
    std::vector<float> in(size_t(620.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(20.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (float era : {0.1f, 0.3f, 0.5f, 0.7f, 0.9f}) {
        Setting s;
        s.era = era;
        s.decay = 1.0f;
        const Stereo out = render(in, s);
        const double win = era > 0.8f ? 25.0 : 5.0; // the radio: average its fades
        const double first = rmsDb(out.l, 60.0, 60.0 + std::fmax(10.0, win));
        double lo = first, hi = first;
        for (double t = 60.0; t + win < 620.0; t += 30.0) {
            const double db = rmsDb(out.l, t, t + win);
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
        }
        char msg[160];
        std::snprintf(msg, sizeof msg, "infinite holds 10 min at ERA %.2f: %.1f .. %.1f dBFS (%.0f s windows)", era, lo, hi, win);
        check(finite(out) && hi - lo < 3.0 && hi <= first + 1.0 && lo > -60.0, msg);
    }
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

// Turning ERA through the eras doesn't click: through the Midiverb's, the BBD's and the cassette's starts
// (where tape out comes in: switched in one sample, its oversampling filters' phase stepped the wet, found
// 25 Oct 2026). The radio's crackle is clicks by design, so its start isn't judged here.
void eraSweepClickFree()
{
    std::vector<float> in(size_t(6.0f * kSr));
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / kSr) + 0.1f * std::sin(2.0f * 3.14159265f * 331.0f * float(i) / kSr);
    for (int mode : {0, 1, 2}) { // up, down, and down then back up (the tape comes back in after resting)
        const bool up = mode == 0;
        iv::Engine e;
        e.prepare(kSr, 48);
        e.setParam(iv::ParamId::Space, iv::switchToNormalised(1));
        e.setParam(iv::ParamId::Mix, 1.0f);
        e.setParam(iv::ParamId::Era, up ? 0.1f : 0.75f);
        e.reset();
        Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
        for (size_t pos = 0; pos < in.size(); pos += 48) {
            const float sec = float(pos) / kSr;
            const float t = std::fmin(1.0f, std::fmax(0.0f, (sec - 1.0f) / 8.0f)); // 1 s still, 8 s across 0.10-0.75
            float era = up ? 0.1f + 0.65f * t : 0.75f - 0.65f * t;
            if (mode == 2) era = sec < 5.0f ? 0.75f - 0.25f * std::fmin(1.0f, std::fmax(0.0f, sec - 1.0f) / 2.0f) // to the BBD
                                            : 0.5f + 0.25f * std::fmin(1.0f, (sec - 5.0f) / 2.0f);                 // and back
            
            e.setParam(iv::ParamId::Era, era);
            const int n = int(std::min<size_t>(48, in.size() - pos));
            e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        }
        const auto m = iv::metrics::compute({out.l, out.r}, kSr);
        check(finite(out) && m.clickCount == 0, std::string("ERA turned ") + (mode == 0 ? "up" : mode == 1 ? "down" : "down and back up") +
                                                    " through the eras' starts, no clicks (" + std::to_string(m.clickCount) + ")");
    }
}

// SPEC §7 M6: the saturation hit hard doesn't alias badly. A loud 7 kHz
// tone's 3rd/5th harmonics fold to 27 → 21 kHz and 35 → 13 kHz: check the
// band below 5 kHz, where nothing harmonic belongs. Tape out's asymmetry
// (v1.0.22, even harmonics as tape's) puts real difference tones there too, so
// the same tone is also rendered at 96 kHz, where little folds: aliasing is
// what 48 kHz adds over that (or the band simply stays 40 dB down).
double lowBandDb(float sr)
{
    std::vector<float> in(size_t(3.0f * sr));
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.9f * std::sin(2.0f * 3.14159265f * 7000.0f * float(i) / sr);
    Setting s;
    s.era = 0.75f; // cassette drive
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
    return 10.0 * std::log10(lowBand / total + 1e-30);
}

void notAliasing()
{
    const double at48 = lowBandDb(kSr), at96 = lowBandDb(96000.0f);
    check(at48 < -40.0 || at48 - at96 < 2.0,
          "hot 7 kHz tone at the cassette stage: energy below 5 kHz " + std::to_string(int(at48)) + " dB re total at 48 kHz, " +
              std::to_string(int(at96)) + " at 96 kHz (< −40, or aliasing adds < 2 dB)");
}

} // namespace

int main()
{
    eraDarkens();
    eraLoudness();
    erasOnWetOnly();
    midiverbReachesSilence();
    hissFollowsInput();
    infiniteAtEveryEra();
    drift();
    eraSweepClickFree();
    notAliasing();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
