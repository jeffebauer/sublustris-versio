// The SIZE blast (v1.0.77, ADR 0053, the owner's pick C on renders/size_blast; issue 2, a fast SIZE move from big to small
// "blasts the speaker with very loud audio"). SIZE's crossfade turns the new, nearer read up for one trip, by the tail's own
// decay over the difference: right for a dying tail, real gain in the loop while the input keeps coming, and it had no limit.
// A shrink's boost is limited to about 6 dB, and a tank whose numbers go invalid restarts.
//
// Asserted, 100 % wet, sustained hot material, SIZE flicked 1.0 to the size below over 40 ms:
//   - the wet's rise over its steady level stays bounded (the limits are under v1.0.76's own figures, in the comments);
//   - moves that used to end in NaN stay finite and keep sounding;
//   - a NaN put into each tank's state is caught, gives no NaN out, and the reverb builds again;
//   - a held setting never restarts.
// Normal moves and growths staying as they were is the job of the goldens and tools/same_render.py.

#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

constexpr float  kSr = 48000.0f;
constexpr int    kBlock = 48;
constexpr double kTwoPi = 6.283185307179586;

enum Kind { Bowed, Pad, Bass };

struct Scene {
    int   space, onset;
    float decay, tone, wear, sizeTo;
    Kind  kind;
    float level;
};

struct Out {
    double steady = 0.0, peak = 0.0, last = 0.0; // 50 ms RMS, dB: the steady level, the loudest after the move, the last second's
    bool   finite = true;
    int    restarts = 0;
};

double db(double x) { return 20.0 * std::log10(std::max(1e-12, x)); }

// The material, peaking at s.level: a bowed line (a harmonic stack with vibrato), a detuned saw pad, or a steady low note.
std::vector<float> material(const Scene& s, int n)
{
    std::vector<float> x(size_t(n), 0.0f);
    double ph[8] = {};
    float lp = 0.0f;
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / double(kSr);
        if (s.kind == Bass) {
            x[size_t(i)] = float(std::sin(kTwoPi * 70.0 * t));
        } else if (s.kind == Pad) {
            static constexpr double kNotes[4] = {110.0, 164.81, 220.0, 277.18};
            float h = 0.0f;
            for (int v = 0; v < 8; ++v) {
                ph[v] += kNotes[v / 2] * (1.0 + 0.002 * double(v % 2)) / double(kSr);
                if (ph[v] > 1.0) ph[v] -= 1.0;
                h += float(2.0 * ph[v] - 1.0);
            }
            lp += 0.15f * (h - lp);
            x[size_t(i)] = lp;
        } else {
            static constexpr double kNotes[4] = {440.0, 587.33, 659.25, 392.0};
            const double f0 = kNotes[int(t / 2.5) % 4] * (1.0 + 0.006 * std::sin(kTwoPi * 5.5 * t));
            ph[0] += f0 / double(kSr);
            if (ph[0] > 1.0) ph[0] -= 1.0;
            double h = 0.0;
            for (int m = 1; m <= 16; ++m) h += std::sin(kTwoPi * double(m) * ph[0]) / double(m);
            x[size_t(i)] = float(h);
        }
    }
    float pk = 1e-9f;
    for (float v : x) pk = std::max(pk, std::fabs(v));
    for (float& v : x) v *= s.level / pk;
    return x;
}

// Settles for 10 s at SIZE 1, moves, keeps playing for 5 s. poisonTank >= 0 puts a NaN in that tank just before the move and
// holds SIZE still.
Out run(iv::Engine& e, const Scene& s, int poisonTank = -1, bool hold = false)
{
    e.setParam(iv::ParamId::Mix, 1.0f);
    e.setParam(iv::ParamId::Decay, s.decay);
    e.setParam(iv::ParamId::Tone, s.tone);
    e.setParam(iv::ParamId::Era, s.wear);
    e.setParam(iv::ParamId::Drift, 0.5f);
    e.setParam(iv::ParamId::Time, 0.5f);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Attack, iv::switchToNormalised(s.onset));
    e.setParam(iv::ParamId::Size, 1.0f);
    e.reset();

    const int moveBlock = int(10.0f * kSr) / kBlock, moveBlocks = int(0.040f * kSr) / kBlock;
    const int total = int(15.0f * kSr) / kBlock;
    const int frame = 50; // blocks: 50 ms
    const std::vector<float> src = material(s, total * kBlock);
    float l[kBlock], r[kBlock];
    double acc = 0.0;
    int inFrame = 0, n = 0;
    Out o;
    std::vector<double> frames;
    for (int b = 0; b < total; ++b) {
        const float* in = src.data() + b * kBlock;
        if (b == moveBlock && poisonTank >= 0) e.poisonTankForTests(poisonTank);
        if (!hold && poisonTank < 0 && b >= moveBlock && b <= moveBlock + moveBlocks)
            e.setParam(iv::ParamId::Size, 1.0f + (s.sizeTo - 1.0f) * float(b - moveBlock) / float(moveBlocks));
        e.process(in, in, l, r, kBlock);
        for (int i = 0; i < kBlock; ++i) {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) o.finite = false;
            acc += 0.5 * (double(l[i]) * l[i] + double(r[i]) * r[i]);
        }
        n += kBlock;
        if (++inFrame == frame) {
            frames.push_back(std::sqrt(acc / double(n)));
            acc = 0.0;
            n = inFrame = 0;
        }
    }
    const size_t movedAt = size_t(moveBlock / frame);
    for (size_t i = movedAt - 80; i < movedAt; ++i) o.steady = std::max(o.steady, frames[i]); // the 4 s before the move
    for (size_t i = movedAt; i < frames.size(); ++i) o.peak = std::max(o.peak, frames[i]);
    for (size_t i = frames.size() - 20; i < frames.size(); ++i) o.last = std::max(o.last, frames[i]);
    o.steady = db(o.steady);
    o.peak = db(o.peak);
    o.last = db(o.last);
    o.restarts = e.tankRestartsForTests();
    return o;
}

} // namespace

int main()
{
    auto eng = std::make_unique<iv::Engine>();
    iv::Engine& e = *eng;
    e.prepare(kSr, kBlock);
    char buf[200];

    // The player's patch and the worst short-decay ones: the rise over the steady level.
    struct Surge { const char* name; Scene s; double maxRiseDb; };
    const Surge surges[] = {
        {"CANYON BLOOM DECAY 0.85 bowed line (the report)", {2, 1, 0.85f, 0.5f, 0.5f, 0.05f, Bowed, 1.0f}, 3.0},
        {"CANYON NATURAL DECAY 0.1 WEAR full pad", {2, 0, 0.1f, 0.5f, 1.0f, 0.2f, Pad, 0.3f}, 5.0},
        {"HALL NATURAL DECAY 0 pad", {1, 0, 0.0f, 0.5f, 0.5f, 0.05f, Pad, 0.3f}, 2.0},
    };
    for (const Surge& t : surges) {
        const Out o = run(e, t.s);
        std::snprintf(buf, sizeof buf, "%s: rises %+.1f dB (at most %+.1f), finite", t.name, o.peak - o.steady, t.maxRiseDb);
        check(o.finite && o.peak - o.steady <= t.maxRiseDb, buf);
    }

    // Moves that used to go NaN and stay there: hot bass into CANYON at WEAR full.
    const Scene hot[] = {
        {2, 0, 0.85f, 0.5f, 1.0f, 0.2f, Bass, 1.0f},
        {2, 0, 0.0f, 0.5f, 1.0f, 0.05f, Bass, 1.0f},
    };
    for (const Scene& s : hot) {
        const Out o = run(e, s);
        std::snprintf(buf, sizeof buf, "hot bass, DECAY %.2f, WEAR full, SIZE to %.2f: finite, still sounding (last second %.1f dB)", s.decay, s.sizeTo, o.last);
        check(o.finite && o.last > -60.0, buf);
    }

    // A NaN in each tank: caught, nothing invalid out, and the reverb comes back while the input carries on.
    struct Poison { const char* name; int tank, space; };
    const Poison poisons[] = {{"the FDN (HALL)", 0, 1}, {"the still lows (CANYON)", 1, 2}, {"the plate (PLATE)", 2, 0}};
    for (const Poison& t : poisons) {
        const int before = e.tankRestartsForTests();
        const Scene s{t.space, 0, 0.5f, 0.5f, 0.5f, 1.0f, Pad, 0.5f};
        const Out o = run(e, s, t.tank);
        std::snprintf(buf, sizeof buf, "NaN in %s: no NaN out, %d restart(s), last second %.1f dB", t.name, o.restarts - before, o.last);
        check(o.finite && o.restarts - before == 1 && o.last > o.steady - 12.0, buf);
    }

    // Held still, nothing restarts.
    {
        const int before = e.tankRestartsForTests();
        const Out o = run(e, {2, 1, 0.85f, 0.5f, 0.5f, 1.0f, Bowed, 1.0f}, -1, true);
        std::snprintf(buf, sizeof buf, "SIZE held, hot: no restart (%d), finite", o.restarts - before);
        check(o.finite && o.restarts == before, buf);
    }

    std::printf(failures ? "\n%d check(s) FAILED\n" : "\nAll checks passed.\n", failures);
    return failures ? 1 : 0;
}
