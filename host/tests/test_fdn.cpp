// M1: the FDN Hall (SPEC §4.3, §7 M1). Stability at every DECAY × SIZE ×
// TONE corner, T60 against the DECAY mapping, no metallic ringing, SIZE
// moves glide (at most a semitone of bend, no clicks), determinism, the
// same tail at 44.1 / 48 / 96 kHz, and true stereo in (ADR 0013).

#include "Metrics.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

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

// SIZE glides: a full sweep on a held sound has no clicks, and no line
// ever changes length faster than a semitone of bend.
void sizeSweepGlides()
{
    const float sr = 48000.0f;
    const size_t n = size_t(8.0f * sr);
    std::vector<float> in(n), ramp(n);
    for (size_t i = 0; i < n; ++i) {
        in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / sr) + 0.02f * noise();
        const float t = float(i) / sr;
        ramp[i] = t < 1.0f ? 0.0f : (t < 3.0f ? (t - 1.0f) * 0.5f : (t < 5.0f ? 1.0f - (t - 3.0f) * 0.5f : 0.0f));
    }
    const Stereo out = render(in, {0.6f, 0.0f, 0.5f, 1.0f}, sr, false, &ramp);
    const auto m = iv::metrics::compute({out.l, out.r}, sr);
    check(finite(out) && m.clickCount == 0, "SIZE swept 0 -> 1 -> 0 on a held tone: no clicks (" +
                                                std::to_string(m.clickCount) + ")");

    iv::Engine e;
    e.prepare(sr, 48);
    e.setParam(iv::ParamId::Size, 0.0f);
    e.reset();
    std::vector<float> z(48, 0.0f), ol(48), orr(48);
    e.setParam(iv::ParamId::Size, 1.0f);
    float worst = 0.0f;
    float prev[iv::dsp::Fdn::kN];
    for (int i = 0; i < iv::dsp::Fdn::kN; ++i) prev[i] = e.fdn().lineSeconds(i);
    for (int b = 0; b < 2000; ++b) {
        e.process(z.data(), z.data(), ol.data(), orr.data(), 48);
        for (int i = 0; i < iv::dsp::Fdn::kN; ++i) {
            const float now = e.fdn().lineSeconds(i);
            worst = std::fmax(worst, std::fabs(now - prev[i]) / (48.0f / sr)); // seconds of length per second
            prev[i] = now;
        }
    }
    char msg[160];
    std::snprintf(msg, sizeof msg, "SIZE jump glides: fastest length change %.4f s/s (<= one semitone, 0.0595)", worst);
    check(worst <= iv::voicing::kSizeSlewPerSecond + 1e-4f, msg);
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

} // namespace

int main()
{
    stabilityGrid();
    t60FollowsDecay();
    notMetallic();
    sizeSweepGlides();
    deterministic();
    sampleRates();
    stereo();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
