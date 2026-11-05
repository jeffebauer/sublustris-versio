// v1.0.44: TONE's resonant tilt (dsp/Output.h ToneBell; owner's pick A, renders/tone_character). Pins its shape (a
// +4.5 dB bell at 600 Hz fully CCW and at 5 kHz fully CW, on top of the make-up; noon passes the Tilt's output bit for
// bit), its level against the wet without it on music (within ±1 dB across TONE), no clicks on TONE CV sweeps
// (0.1, 2 and 20 Hz, 1 ms frames against their neighbours, no worse than the wet without it), and the loop
// untouched (infinite and frozen tanks bit-identical with and without it).

#include "DecayKnobFor.h"
#include "Wav.h"
#include "dsp/Engine.h"
#include "dsp/Output.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstdio>
#include <cstring>
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

// The bell's steady gain (dB) for a sine at f, TONE fixed.
double bellGainDb(float tone, float f)
{
    iv::dsp::ToneBell b;
    b.prepare(kSr);
    b.set(tone, 32);
    b.snap();
    double in = 0.0, out = 0.0;
    const int n = int(0.4f * kSr);
    for (int i = 0; i < n; ++i) {
        if (i % 32 == 0) b.set(tone, 32);
        const float x = 0.25f * std::sin(6.2831853f * f * float(i) / kSr);
        float l = x, r = x;
        b.process(l, r);
        if (i >= n / 2) {
            in += double(x) * x;
            out += double(l) * l;
        }
    }
    return 10.0 * std::log10(out / in);
}

// The make-up table carries the wet's flat +3 dB since v1.0.73 (ADR 0049); these tests are about the bell, so they take it off.
constexpr double kWetLiftDb = 3.0;

void bellShape()
{
    struct End { float tone, corner, far, lo, hi; double makeupDb; const char* name; };
    for (const End& e : {End{0.0f, 600.0f, 60.0f, 400.0f, 900.0f, -2.26 + kWetLiftDb, "fully CCW"},
                         End{1.0f, 5000.0f, 300.0f, 3000.0f, 8000.0f, -0.20 + kWetLiftDb, "fully CW"}}) {
        double best = -99.0, at = 0.0;
        for (float f = e.lo; f <= e.hi; f *= 1.01f) {
            const double g = bellGainDb(e.tone, f);
            if (g > best) { best = g; at = f; }
        }
        const double far = bellGainDb(e.tone, e.far);
        char msg[240];
        std::snprintf(msg, sizeof msg, "TONE %s: the bell peaks %+.2f dB over its make-up at %.0f Hz (+4.5 dB at %.0f Hz, "
                      "within 0.3 dB and 5 %%); far from it %+.2f dB (the make-up, %+.2f)",
                      e.name, best - e.makeupDb, at, e.corner, far, e.makeupDb);
        check(std::fabs(best - e.makeupDb - 4.5) < 0.3 && std::fabs(at / e.corner - 1.0) < 0.05
                  && std::fabs(far - e.makeupDb) < 0.15, msg);
    }
    // Noon: the Tilt's output and the flat +3 dB, nothing else (the bell's sum is mk x + 0 + 0).
    iv::dsp::ToneBell b;
    b.prepare(kSr);
    b.set(0.5f, 32);
    b.snap();
    const float mk = float(std::pow(10.0, kWetLiftDb / 20.0));
    uint32_t seed = 7;
    bool same = true;
    for (int i = 0; i < 48000; ++i) {
        if (i % 32 == 0) b.set(0.5f, 32);
        seed = seed * 1664525u + 1013904223u;
        const float x = float(int32_t(seed)) / 2147483648.0f;
        float l = x, r = -x;
        b.process(l, r);
        same = same && std::fabs(l - mk * x) <= 1e-6f && r == -l;
    }
    check(same, "TONE noon: the bell passes the wet, +3 dB and nothing else");
}

struct Stereo {
    std::vector<float> l, r;
};

// The Engine on in (mono), 48-sample blocks; toneAt(t) sets TONE each block (a CV), freezeAt > 0 taps the button.
template <typename ToneAt>
Stereo render(const std::vector<float>& in, int space, float decay, ToneAt toneAt, bool bell, float freezeAt = -1.0f,
              std::vector<float>* tank = nullptr)
{
    iv::Engine e;
    e.prepare(kSr, 48);
    e.setToneBellForTests(bell);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e.setParam(iv::ParamId::Decay, decayKnobFor(decay));
    e.setParam(iv::ParamId::Size, 0.7f);
    e.setParam(iv::ParamId::Era, 0.0f);
    e.setParam(iv::ParamId::Mix, 1.0f);
    e.setParam(iv::ParamId::Time, 0.0f);
    e.setParam(iv::ParamId::Tone, toneAt(0.0));
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const double t = double(pos) / kSr;
        e.setParam(iv::ParamId::Tone, toneAt(t));
        if (freezeAt > 0.0f && pos == size_t(freezeAt * kSr) / 48 * 48) e.button(true, 0);
        if (freezeAt > 0.0f && pos == size_t((freezeAt + 0.1f) * kSr) / 48 * 48) e.button(false, 0);
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        if (tank) {
            tank->push_back(e.tankLevelDb());
            tank->push_back(e.tankSlowLevelDb());
        }
    }
    return out;
}

// BS.1770's K-weighting at 48 kHz, the whole render (as test_era's ERA loudness).
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

void levelOnMusic(const std::vector<float>& skankIn)
{
    std::vector<float> in(skankIn);
    in.resize(in.size() + size_t(4.0f * kSr), 0.0f);
    for (int space : {1, 2}) {
        double worst = 0.0;
        std::string detail;
        for (int k = 0; k <= 8; ++k) {
            const float tone = float(k) / 8.0f;
            auto at = [tone](double) { return tone; };
            const double d = kWeightedDb(render(in, space, 0.75f, at, true)) - kWeightedDb(render(in, space, 0.75f, at, false)) - kWetLiftDb;
            if (std::fabs(d) > std::fabs(worst)) worst = d;
            char buf[32];
            std::snprintf(buf, sizeof buf, " %.3f: %+.2f", tone, d);
            detail += buf;
        }
        check(std::fabs(worst) <= 1.0, std::string(space == 1 ? "HALL" : "MASSIVE")
                                           + ": the resonant tilt keeps music's loudness (LUFS) within ±1 dB of the wet "
                                             "without it across TONE:" + detail);
    }
}

// The worst 1 ms frame of the highs (> 5 kHz, two RBJ high-passes) against its neighbours 2-4 ms each side, dB;
// frames under -90 dBFS (a shut filter's highs) are left out.
double spikeDb(const Stereo& s, double from)
{
    double z[2][2] = {}, y[2][2] = {};
    const double w0 = 6.283185307 * 5000.0 / kSr, al = std::sin(w0) / (2.0 * 0.7071), c = std::cos(w0);
    const double b0 = (1 + c) / 2 / (1 + al), b1 = -(1 + c) / (1 + al), b2 = b0, a1 = -2 * c / (1 + al), a2 = (1 - al) / (1 + al);
    std::vector<double> e;
    double acc = 0.0;
    for (size_t i = 0; i < s.l.size(); ++i) {
        double v = 0.5 * (double(s.l[i]) + s.r[i]);
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

void cvSweeps(const std::vector<float>& skankIn)
{
    for (int space : {1, 2})
        for (double hz : {0.1, 2.0, 20.0}) {
            const double secs = hz < 1.0 ? 10.0 : 8.0;
            std::vector<float> in(skankIn.begin(), skankIn.begin() + std::min(skankIn.size(), size_t((secs + 1.0) * kSr)));
            auto tri = [hz](double t) { // a full-range triangle from noon, after 1 s
                if (t < 1.0) return 0.5f;
                const double p = std::fmod((t - 1.0) * hz + 0.25, 1.0);
                return float(p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p);
            };
            Stereo bell = render(in, space, 0.75f, tri, true); // without the flat +3 dB: spikeDb's -90 dBFS gate reads level
            const float trim = float(std::pow(10.0, -kWetLiftDb / 20.0));
            for (float& v : bell.l) v *= trim;
            for (float& v : bell.r) v *= trim;
            const double on = spikeDb(bell, 1.0);
            const double off = spikeDb(render(in, space, 0.75f, tri, false), 1.0);
            char msg[200];
            std::snprintf(msg, sizeof msg, "%s, TONE CV at %.1f Hz: worst 1 ms spike %.1f dB, %.1f without the bell (at most +3)",
                          space == 1 ? "HALL" : "MASSIVE", hz, on, off);
            check(on <= off + 3.0, msg);
        }
}

void loopUntouched()
{
    std::vector<float> in(size_t(60.0f * kSr), 0.0f);
    uint32_t seed = 3;
    for (size_t i = 0; i < size_t(4.5f * kSr); ++i) {
        seed = seed * 1664525u + 1013904223u;
        in[i] = 0.3f * float(int32_t(seed)) / 2147483648.0f;
    }
    for (int space : {0, 1, 2})
        for (float tone : {0.0f, 1.0f})
            for (bool freeze : {false, true}) {
                std::vector<float> a, b;
                auto at = [tone](double) { return tone; };
                render(in, space, freeze ? 0.75f : 1.0f, at, true, freeze ? 4.0f : -1.0f, &a);
                render(in, space, freeze ? 0.75f : 1.0f, at, false, freeze ? 4.0f : -1.0f, &b);
                const bool same = a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
                char msg[200];
                std::snprintf(msg, sizeof msg, "%s %s, TONE %.0f, 60 s: the tank's levels bit-identical with and without the bell",
                              space == 0 ? "PLATE" : space == 1 ? "HALL" : "MASSIVE", freeze ? "frozen" : "infinite", tone);
                check(same, msg);
            }
}

} // namespace

int main()
{
    bellShape();
    std::vector<float> sk;
    if (!skank(sk)) {
        check(false, "TONE on music: test_audio/stimulus/04_skank.wav (python3 tools/make_stimulus.py)");
    } else {
        levelOnMusic(sk);
        cvSweeps(sk);
    }
    loopUntouched();
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
