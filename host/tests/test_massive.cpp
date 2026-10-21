// M4: MASSIVE and ATTACK (SPEC §4.4, §4.5, §4.11, §7 M4; ADR 0007, 0008,
// 0013, 0015). BLOOM's rise follows TIME in every space; MASSIVE's gentle
// built-in mass follows DECAY and SIZE; HALL in NORMAL attacks at once;
// infinite tails hold a floor for 10 minutes without growing; MASSIVE's
// SIZE goes from echoes to wash; REVERSE plays each segment backwards,
// rising to its cut; SPACE flips are click-free; HALL and MASSIVE match in
// loudness; MASSIVE is stable at every corner.

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

uint32_t gSeed = 11;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}

struct Stereo {
    std::vector<float> l, r;
};

struct Setting {
    int   space = 1, attack = 0;
    float time = 0.0f, decay = 0.6f, size = 0.5f, mix = 1.0f;
    float era = 0.0f; // clean: these are M4's checks (ATTACK, MASSIVE, the floor); ERA has its own (test_era)
};

// spaceAt: optional SPACE position per sample (automation).
Stereo render(const std::vector<float>& in, const Setting& s, const std::vector<int>* spaceAt = nullptr)
{
    iv::Engine e;
    e.prepare(kSr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Attack, iv::switchToNormalised(s.attack));
    e.setParam(iv::ParamId::Time, s.time);
    e.setParam(iv::ParamId::Decay, s.decay);
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Mix, s.mix);
    e.setParam(iv::ParamId::Era, s.era);
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        if (spaceAt) e.setParam(iv::ParamId::Space, iv::switchToNormalised((*spaceAt)[pos]));
        const int n = int(std::min<size_t>(48, in.size() - pos));
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

// RMS envelope in windows of `win` samples, linear.
std::vector<double> envelope(const Stereo& s, size_t win)
{
    std::vector<double> e;
    for (size_t p = 0; p + win <= s.l.size(); p += win) {
        double sum = 0.0;
        for (size_t i = 0; i < win; ++i) sum += double(s.l[p + i]) * s.l[p + i] + double(s.r[p + i]) * s.r[p + i];
        e.push_back(std::sqrt(sum / double(2 * win)));
    }
    return e;
}

bool finite(const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}

// 10 → 90 % rise of the wet's envelope (10 ms windows) on a 50 ms burst.
double riseSeconds(const Setting& s)
{
    std::vector<float> in(size_t(14.0f * kSr), 0.0f);
    for (size_t i = 0; i < 2400; ++i) in[i] = 0.5f * noise();
    const auto e = envelope(render(in, s), 480);
    size_t pk = 0;
    for (size_t i = 0; i < e.size(); ++i)
        if (e[i] > e[pk]) pk = i;
    size_t a = 0, b = 0;
    for (size_t i = 0; i <= pk; ++i) {
        if (!a && e[i] > 0.1 * e[pk]) a = i;
        if (!b && e[i] > 0.9 * e[pk]) b = i;
    }
    return double(b - a) * 0.01;
}

void attackFollowsTime()
{
    for (int space : {1, 2})
        for (float t : {0.3f, 0.6f, 1.0f}) {
            Setting s;
            s.space = space;
            s.attack = 1;
            s.time = t;
            const double want = iv::map::bloomSeconds(t) * (space == 2 ? iv::map::massiveMass(s.size) : 1.0f);
            const double got = riseSeconds(s);
            char msg[160];
            std::snprintf(msg, sizeof msg, "BLOOM in %s, TIME %.1f: rise %.2f s, target %.2f s (within 20%%)",
                          space == 2 ? "MASSIVE" : "HALL", t, got, want);
            check(std::fabs(got / want - 1.0) < 0.2, msg);
        }
    for (float d : {0.0f, 0.7f}) {
        Setting s;
        s.space = 2;
        s.decay = d;
        const double want = iv::map::massiveNormalAttackSeconds(d, s.size), got = riseSeconds(s);
        char msg[160];
        std::snprintf(msg, sizeof msg, "MASSIVE NORMAL mass at DECAY %.1f: rise %.2f s, target %.2f s (within 20%%)", d, got, want);
        check(std::fabs(got / want - 1.0) < 0.2, msg);
    }
    Setting hall;
    const double h = riseSeconds(hall);
    check(h < 0.1, "HALL NORMAL attacks at once: rise " + std::to_string(h) + " s (< 0.1)");
}

// ADR 0008: fed hard for 20 s at infinite, the tank sits near the floor
// (its own level: the wet is ducked by DARKEN while you play, ADR 0014),
// then the tail holds for 10 minutes: within ±3 dB, not growing past its
// settled level (the hold makeup may lift a sag back to it), never silent.
void infiniteHoldsTheFloor()
{
    std::vector<float> in(size_t(620.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(20.0f * kSr); ++i) in[i] = 0.3f * noise();
    for (int space : {1, 2}) {
        iv::Engine e;
        e.prepare(kSr, 48);
        e.setParam(iv::ParamId::Space, iv::switchToNormalised(space));
        e.setParam(iv::ParamId::Decay, 1.0f);
        e.setParam(iv::ParamId::Size, 0.5f);
        e.setParam(iv::ParamId::Mix, 1.0f);
        e.setParam(iv::ParamId::Era, 0.0f);
        e.setParam(iv::ParamId::Time, 0.0f);
        e.reset();
        Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
        double fedTank = 0.0;
        for (size_t pos = 0; pos < in.size(); pos += 48) {
            e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, 48);
            if (pos == size_t(19.0f * kSr)) fedTank = e.tankLevelDb();
        }
        const auto env = envelope(out, size_t(10.0f * kSr)); // 10 s windows
        const double first = 20.0 * std::log10(env[6]);   // 60-70 s
        double lo = first, hi = first;
        for (size_t i = 6; i < env.size(); ++i) {
            const double db = 20.0 * std::log10(env[i] + 1e-12);
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
        }
        const std::string name = space == 2 ? "MASSIVE" : "HALL";
        char msg[200];
        std::snprintf(msg, sizeof msg, "%s infinite, fed hard: the tank at %.1f dBFS (floor %.0f, within 3 dB)", name.c_str(),
                      fedTank, double(iv::voicing::kFloorDbfs));
        check(finite(out) && std::fabs(fedTank - iv::voicing::kFloorDbfs) < 3.0, msg);
        std::snprintf(msg, sizeof msg, "%s infinite holds 10 min: %.1f .. %.1f dBFS (within 3 dB, no growth, not silent)",
                      name.c_str(), lo, hi);
        check(hi - lo < 3.0 && hi <= first + 1.0 && lo > -60.0, msg);
    }
}

// ADR 0007: low SIZE = distinct repeating echoes (a spiky envelope), high
// SIZE = a wash (smooth). Spikiness: how far the 10 ms envelope strays from
// its own 300 ms average, over 4 s of tail.
double spikinessDb(const Stereo& s)
{
    auto lin = envelope(s, 480);
    std::vector<double> e(lin.size());
    double mx = -300.0;
    for (size_t i = 0; i < lin.size(); ++i) {
        e[i] = 20.0 * std::log10(lin[i] + 1e-10);
        mx = std::fmax(mx, e[i]);
    }
    size_t on = 0;
    while (on < e.size() && e[on] < mx - 30.0) ++on;
    double ss = 0.0;
    int n = 0;
    for (size_t i = on + 50; i < on + 450 && i + 15 < e.size(); ++i) {
        double m = 0.0;
        for (int k = -15; k <= 15; ++k) m += e[i + size_t(k)];
        m /= 31.0;
        ss += (e[i] - m) * (e[i] - m);
        ++n;
    }
    return std::sqrt(ss / double(n));
}

void echoesToWash()
{
    std::vector<float> in(size_t(9.0f * kSr), 0.0f);
    in[1000] = 0.8f;
    std::vector<double> sp;
    std::string detail;
    for (float z : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        Setting s;
        s.space = 2;
        s.decay = 0.7f;
        s.size = z;
        sp.push_back(spikinessDb(render(in, s)));
        char buf[40];
        std::snprintf(buf, sizeof buf, " %.2f", sp.back());
        detail += buf;
    }
    bool falling = true;
    for (size_t i = 1; i < sp.size(); ++i) falling &= sp[i] < sp[i - 1] + 0.05;
    check(falling && sp.front() > 2.0 && sp.back() < 0.6,
          "MASSIVE SIZE 0 -> 1 goes from echoes to wash (spikiness dB:" + detail + "; > 2 at 0, < 0.6 at 1)");
}

// REVERSE (ADR 0015): nothing for the first segment (the reversed wet is
// TIME late), then each segment rises to its cut; no clicks at the cuts.
void reverseSwells()
{
    for (float t : {0.0f, 0.5f}) {
        const double segment = iv::map::reverseSeconds(t);
        std::vector<float> in(size_t(4.0f * kSr), 0.0f);
        in[10] = 0.8f;
        Setting s;
        s.attack = 2;
        s.time = t;
        // A tail that falls >= 10 dB across one segment, so a reversed rise
        // is measurable (a 0.2 s segment of a 4.5 s tail only moves ~3 dB).
        s.decay = t == 0.0f ? 0.0f : 0.3f;
        const Stereo out = render(in, s);
        const size_t L = size_t(segment * kSr);
        double before = 0.0;
        for (size_t i = 0; i + 1 < L; ++i) before = std::fmax(before, std::fabs(out.l[i]));
        int rising = 0, segs = 0;
        for (size_t k = 2; (k + 1) * L <= out.l.size() && k < 6; ++k, ++segs) {
            double q1 = 0.0, q4 = 0.0;
            for (size_t i = k * L; i < k * L + L / 4; ++i) q1 += double(out.l[i]) * out.l[i];
            for (size_t i = (k + 1) * L - L / 4; i < (k + 1) * L; ++i) q4 += double(out.l[i]) * out.l[i];
            rising += q4 > 2.0 * q1;
        }
        char msg[200];
        std::snprintf(msg, sizeof msg, "REVERSE %.2f s: silent for the first segment (peak %.1g), %d of %d segments rise to their cut",
                      segment, before, rising, segs);
        check(before < 1e-6 && segs > 0 && rising == segs, msg);
    }
    std::vector<float> held(size_t(6.0f * kSr));
    for (auto& x : held) x = 0.2f * std::sin(0.03f * float(&x - held.data())) + 0.02f * noise();
    Setting s;
    s.attack = 2;
    s.time = 0.3f;
    s.mix = 1.0f;
    const Stereo out = render(held, s);
    const auto m = iv::metrics::compute({out.l, out.r}, kSr);
    check(finite(out) && m.clickCount == 0, "REVERSE on a held tone: no clicks at the cuts (" + std::to_string(m.clickCount) + ")");
}

// ADR 0013: SPACE flips mid-tail crossfade without clicks.
void spaceFlips()
{
    const size_t n = size_t(10.0f * kSr);
    std::vector<float> in(n);
    std::vector<int> at(n);
    for (size_t i = 0; i < n; ++i) {
        in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 196.0f * float(i) / kSr) + 0.02f * noise();
        const float t = float(i) / kSr;
        at[i] = t < 2.0f ? 1 : (t < 4.0f ? 2 : (t < 6.0f ? 0 : (t < 8.0f ? 2 : 1)));
    }
    Setting s;
    s.mix = 0.5f;
    const Stereo out = render(in, s, &at);
    const auto m = iv::metrics::compute({out.l, out.r}, kSr);
    check(finite(out) && m.clickCount == 0, "SPACE flipped HALL/MASSIVE/PLATE mid-tail: no clicks (" + std::to_string(m.clickCount) + ")");
}

// SPEC §7 M5 (HALL and MASSIVE now; the Plate joins at M5): loudness
// matched within ±1.5 dB, at both ends of SIZE.
void loudnessMatched()
{
    std::vector<float> in(size_t(12.0f * kSr), 0.0f);
    for (int k = 0; k < 4; ++k)
        for (size_t i = 0; i < 12000; ++i) in[size_t(k) * 96000 + i] = 0.3f * noise();
    double lo = 1e9, hi = -1e9;
    std::string detail;
    for (int space : {1, 2})
        for (float z : {0.0f, 1.0f}) {
            Setting s;
            s.space = space;
            s.size = z;
            s.decay = 0.5f;
            const double db = 20.0 * std::log10(envelope(render(in, s), in.size())[0]);
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
            char buf[48];
            std::snprintf(buf, sizeof buf, " %s/%.0f %.1f", space == 2 ? "M" : "H", z, db);
            detail += buf;
        }
    check(hi - lo <= 3.0, "HALL and MASSIVE loudness within ±1.5 dB:" + detail);
}

void massiveStable()
{
    std::vector<float> in(size_t(8.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.5f * kSr); ++i) in[i] = 0.3f * noise();
    int bad = 0, total = 0;
    for (float d : {0.0f, 0.8f, 1.0f})
        for (float z : {0.0f, 0.5f, 1.0f})
            for (int a : {0, 1, 2})
                for (float t : {0.0f, 1.0f}) {
                    ++total;
                    Setting s;
                    s.space = 2;
                    s.decay = d;
                    s.size = z;
                    s.attack = a;
                    s.time = t;
                    const Stereo out = render(in, s);
                    float pk = 0.0f;
                    for (size_t i = 0; i < out.l.size(); ++i) pk = std::fmax(pk, std::fabs(out.l[i]));
                    if (!finite(out) || pk > 2.0f) ++bad;
                }
    check(bad == 0, "MASSIVE stable at all " + std::to_string(total) + " DECAY x SIZE x ATTACK x TIME corners");
}

} // namespace

int main()
{
    attackFollowsTime();
    infiniteHoldsTheFloor();
    echoesToWash();
    reverseSwells();
    spaceFlips();
    loudnessMatched();
    massiveStable();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
