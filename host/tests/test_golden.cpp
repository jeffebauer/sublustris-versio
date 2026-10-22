// Golden renders (docs/research/reverb-testing-practices.md, item 14; owner's
// testing plan, 22 Oct 2026): a fingerprint of ten settings across the
// SPACEs, SIZE ends, TONE, ERA and ATTACK, each fed a click and a short
// chord, is kept in host/tests/golden/fingerprints.json. A change that
// moves any of them fails here, so a fix in one place can't quietly change
// a sound the owner already approved somewhere else.
//
// Changing a sound on purpose: add the SPEC changelog line first, then
//   IV_GOLDEN_UPDATE="why it changed" build/iv_test_golden
// rewrites the file. It refuses unless SPEC.md's newest changelog version
// is newer than the one the file was last written under, so every golden
// change has its changelog line.

#include "Fft.h"
#include "Json.h"
#include "Metrics.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
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

std::string here()
{
    const std::string f = __FILE__;
    return f.substr(0, f.find_last_of('/') + 1);
}
const std::string kGoldenPath = here() + "golden/fingerprints.json";
const std::string kSpecPath = here() + "../../SPEC.md";

struct Corner {
    const char* name;
    int         space, attack;
    float       size, decay, tone, era, drift, time;
};

// Era −1 = its default. Each corner is one the owner has heard on a page.
const Corner kCorners[] = {
    {"plate_small",     0, 0, 0.0f, 0.4f, 0.5f, -1.0f, 0.5f, 0.2f},
    {"plate_large",     0, 0, 1.0f, 0.7f, 0.5f, -1.0f, 0.5f, 0.2f},
    {"plate_bright",    0, 0, 0.5f, 0.5f, 1.0f, -1.0f, 0.5f, 0.2f},
    {"hall_small",      1, 0, 0.0f, 0.3f, 0.5f, -1.0f, 0.5f, 0.2f},
    {"hall_large",      1, 0, 1.0f, 0.7f, 0.5f, -1.0f, 0.5f, 0.2f},
    {"hall_dark",       1, 0, 0.5f, 0.5f, 0.0f, -1.0f, 0.5f, 0.2f},
    {"hall_midiverb",   1, 0, 0.5f, 0.5f, 0.5f, 1.0f,  0.5f, 0.2f},
    {"massive_small",   2, 0, 0.0f, 0.5f, 0.5f, -1.0f, 0.5f, 0.2f},
    {"massive_large",   2, 0, 1.0f, 0.6f, 0.5f, -1.0f, 0.5f, 0.5f},
    {"massive_reverse", 2, 2, 0.5f, 0.5f, 0.5f, -1.0f, 0.5f, 0.5f},
};

using Stereo = std::vector<std::vector<float>>;

Stereo render(const Corner& c, const std::vector<float>& in)
{
    iv::Engine e;
    e.prepare(kSr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(c.space));
    e.setParam(iv::ParamId::Attack, iv::switchToNormalised(c.attack));
    e.setParam(iv::ParamId::Size, c.size);
    e.setParam(iv::ParamId::Decay, c.decay);
    e.setParam(iv::ParamId::Tone, c.tone);
    if (c.era >= 0.0f) e.setParam(iv::ParamId::Era, c.era);
    e.setParam(iv::ParamId::Drift, c.drift);
    e.setParam(iv::ParamId::Time, c.time);
    e.setParam(iv::ParamId::Mix, 1.0f);
    e.reset();
    Stereo out(2, std::vector<float>(in.size()));
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out[0].data() + pos, out[1].data() + pos, n);
    }
    return out;
}

std::vector<float> click(double seconds)
{
    std::vector<float> x(size_t(seconds * kSr), 0.0f);
    x[size_t(0.1 * kSr)] = 0.5f;
    return x;
}

// An A minor chord with a soft attack, 1 s, then silence: pitched material,
// where a change in colour or ringing shows.
std::vector<float> chord(double seconds)
{
    std::vector<float> x(size_t(seconds * kSr), 0.0f);
    const double f[] = {220.0, 261.63, 329.63, 440.0, 659.26};
    for (size_t i = size_t(0.1 * kSr); i < size_t(1.1 * kSr); ++i) {
        const double t = double(i) / kSr - 0.1;
        const double env = std::min(1.0, t / 0.01) * std::exp(-t * 2.0);
        double s = 0.0;
        for (double h : f) s += std::sin(2.0 * M_PI * h * t) + 0.3 * std::sin(4.0 * M_PI * h * t);
        x[i] = float(0.05 * env * s);
    }
    return x;
}

// Wet loudness (dB, both channels) and energy per octave band 125 Hz–8 kHz,
// relative to the total (dB): the "brightness per band" of the fingerprint.
void spectrum(const Stereo& s, double& loudDb, std::vector<double>& bandDb)
{
    double sum = 0.0;
    for (const auto& ch : s)
        for (float v : ch) sum += double(v) * v;
    loudDb = 10.0 * std::log10(sum / double(2 * s[0].size()) + 1e-30);
    const size_t fftN = 8192;
    std::vector<double> power(fftN / 2 + 1, 0.0);
    for (const auto& ch : s)
        for (size_t at = 0; at + fftN <= ch.size(); at += fftN / 2) {
            const auto mag = iv::fft::magnitudeSpectrum(ch.data() + at, fftN, fftN);
            for (size_t k = 0; k < mag.size(); ++k) power[k] += double(mag[k]) * mag[k];
        }
    double total = 0.0;
    for (double p : power) total += p;
    bandDb.clear();
    for (double hz : iv::metrics::Metrics::bandHz) {
        double e = 0.0;
        for (size_t k = 0; k < power.size(); ++k) {
            const double f = double(k) * kSr / double(fftN);
            if (f >= hz / std::sqrt(2.0) && f < hz * std::sqrt(2.0)) e += power[k];
        }
        bandDb.push_back(10.0 * std::log10(e / (total + 1e-30) + 1e-30));
    }
}

// One corner's fingerprint, as JSON.
iv::json::Value fingerprint(const Corner& c)
{
    using iv::json::Value;
    const auto m = iv::metrics::compute(render(c, click(30.0)), kSr);
    double loud = 0.0;
    std::vector<double> bands;
    spectrum(render(c, chord(12.0)), loud, bands);
    Value v = Value::makeObject();
    auto num = [](double x) { return std::isfinite(x) ? Value::makeNumber(std::round(x * 1000.0) / 1000.0) : Value::makeNull(); };
    Value t60 = Value::makeArray(), band = Value::makeArray();
    for (double x : m.bandT60S) t60.push(num(x));
    for (double x : bands) band.push(num(x));
    v.set("chord_loudness_db", num(loud));
    v.set("chord_band_db", band);
    v.set("band_t60_s", t60);
    v.set("smooth_after_s", num(m.echoDensityTimeS));
    v.set("c80_db", num(m.c80Db));
    return v;
}

// How far a fingerprint value may move before it counts as a change: loudness
// and band balance 0.5 dB, decay 5 %, smoothness 30 ms, clarity 0.5 dB.
bool close(const std::string& key, double was, double now)
{
    if (key == "band_t60_s") return std::fabs(now - was) <= 0.05 * std::fabs(was) + 0.01;
    if (key == "smooth_after_s") return std::fabs(now - was) <= 0.03;
    return std::fabs(now - was) <= 0.5;
}

std::string newestSpecVersion()
{
    std::ifstream f(kSpecPath);
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("- v", 0) == 0) return line.substr(2, line.find_first_of(" \t", 2) - 2);
    return "";
}

// "v1.0.11" > "v1.0.9".
bool newer(const std::string& a, const std::string& b)
{
    auto parts = [](const std::string& s) {
        std::vector<int> p;
        std::stringstream ss(s.size() > 1 ? s.substr(1) : "");
        std::string x;
        while (std::getline(ss, x, '.')) p.push_back(std::atoi(x.c_str()));
        return p;
    };
    return parts(a) > parts(b);
}

} // namespace

int main()
{
    using iv::json::Value;
    Value now = Value::makeObject();
    for (const Corner& c : kCorners) now.set(c.name, fingerprint(c));

    Value golden;
    std::string err;
    const bool have = iv::json::loadFile(kGoldenPath, golden, err);
    const char* why = std::getenv("IV_GOLDEN_UPDATE");
    const std::string spec = newestSpecVersion();

    if (why || !have) {
        const std::string was = have ? golden.get("spec", std::string()) : "";
        if (have && !newer(spec, was)) {
            check(false, "golden update refused: SPEC.md's newest changelog line is " + spec + ", the same as the goldens' " + was +
                             ". Add the changelog line for this sound change first");
        } else {
            Value file = Value::makeObject();
            file.set("spec", Value::makeString(spec));
            file.set("why", Value::makeString(why ? why : "first fingerprints"));
            file.set("corners", now);
            check(iv::json::saveFile(kGoldenPath, file, err), "goldens written under " + spec + " (" + kGoldenPath + ")");
        }
        std::printf("%d failure(s)\n", failures);
        return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const Value* corners = golden.find("corners");
    for (const Corner& c : kCorners) {
        const Value* g = corners ? corners->find(c.name) : nullptr;
        if (!g) { check(false, std::string(c.name) + ": no golden (run with IV_GOLDEN_UPDATE)"); continue; }
        const Value* n = now.find(c.name);
        std::string diffs;
        for (const auto& [key, want] : g->entries()) {
            const Value* got = n->find(key);
            std::vector<const Value*> ws, gs;
            if (want.isArray()) {
                for (const auto& x : want.items()) ws.push_back(&x);
                for (const auto& x : got->items()) gs.push_back(&x);
            } else {
                ws.push_back(&want);
                gs.push_back(got);
            }
            for (size_t i = 0; i < ws.size() && i < gs.size(); ++i) {
                const bool wNull = ws[i]->isNull(), gNull = gs[i]->isNull();
                if (wNull && gNull) continue;
                if (wNull != gNull || !close(key, ws[i]->numberValue(), gs[i]->numberValue())) {
                    char b[160];
                    std::snprintf(b, sizeof b, " %s%s %s -> %s", key.c_str(),
                                  ws.size() > 1 ? ("[" + std::to_string(int(iv::metrics::Metrics::bandHz[i])) + " Hz]").c_str() : "",
                                  wNull ? "null" : std::to_string(ws[i]->numberValue()).c_str(),
                                  gNull ? "null" : std::to_string(gs[i]->numberValue()).c_str());
                    diffs += b;
                }
            }
        }
        check(diffs.empty(), std::string(c.name) + " sounds as approved" + (diffs.empty() ? "" : ":" + diffs));
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
