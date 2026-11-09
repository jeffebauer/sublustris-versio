// SIZE-blast measurement harness (issue 2; dist/size-blast-harness/sb.cpp, with SB_VOICING / SB_CAP_DB / SB_POISON added).
// Build: c++ -std=c++17 -O2 -Icore -Ifirmware tools/pages/size_blast_sb.cpp <build>/libiv_core.a -o <build>/sb Links only core/; compiled once per tree.
// Reads scenarios on stdin, one per line:
//   id space onset decay tone wear motion depth size0 size1 moveMs path material level repeats gapMs
// path: hold | ramp (setParam every block) | knob (release firmware's path)
// Writes one CSV row per scenario. Optional env SB_TRACE=<dir>: writes <id>.trace (50 ms RMS dB series) and <id>.raw (float32 stereo) for ids listed in SB_TRACE_IDS.
#include "dsp/Engine.h"
#include "PotEndStops.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef SB_PROBE
namespace iv { namespace dsp { float g_maxLn=0,g_maxDelta=0,g_rate=0,g_maxK=0; int g_xfs=0; } }
using namespace iv::dsp;
#endif
static const int kBlock = 48;
static const float kFs = 48000.0f;

struct Sc {
    std::string id, path, material;
    int space, onset;
    float decay, tone, wear, motion, depth, size0, size1, moveMs, level;
    int repeats;
    float gapMs;
};

// The material: deterministic, sustained, real level.
static void makeInput(const Sc& s0, int n, std::vector<float>& L, std::vector<float>& R)
{
    // "<material>_stop": the input fades out at 8.9-9.0 s, so a move at 10 s lands on a dying tail.
    Sc s = s0;
    const size_t stopAt = s.material.find("_stop");
    const bool stop = stopAt != std::string::npos;
    if (stop) s.material = s.material.substr(0, stopAt);
    L.assign(n, 0.0f);
    R.assign(n, 0.0f);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    const float twoPi = 6.2831853f;
    double ph[4][12] = {};
    float pinkB[3] = {};
    float lp = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / kFs;
        float x = 0.0f;
        if (s.material == "violin") {
            // Bowed notes 2.4 s long, 0.1 s between them: a sawtooth-rich harmonic stack, vibrato, bow noise.
            const float notes[4] = {440.0f, 587.33f, 659.25f, 392.0f};
            const float T = 2.5f;
            const int k = int(t / T);
            const float nt = t - float(k) * T;
            const float f0 = notes[k % 4] * (1.0f + 0.006f * std::sin(twoPi * 5.5f * t));
            const float env = std::min(1.0f, nt / 0.05f) * std::min(1.0f, std::max(0.0f, (2.4f - nt) / 0.05f));
            double& p = ph[0][0];
            p += f0 / kFs;
            if (p > 1.0) p -= 1.0;
            float h = 0.0f;
            for (int m = 1; m <= 16; ++m) {
                const float a = (1.0f / float(m)) * (1.0f + 1.5f * std::exp(-std::pow(float(m) * f0 - 2800.0f, 2.0f) / 1.5e6f));
                h += a * std::sin(twoPi * float(m) * float(p));
            }
            x = env * (0.55f * h + 0.015f * u(rng));
        } else if (s.material == "pad") {
            const float notes[4] = {110.0f, 164.81f, 220.0f, 277.18f};
            float h = 0.0f;
            for (int v = 0; v < 4; ++v)
                for (int d = 0; d < 2; ++d) {
                    double& p = ph[v][d];
                    p += notes[v] * (1.0f + 0.002f * float(d)) / kFs;
                    if (p > 1.0) p -= 1.0;
                    h += float(2.0 * p - 1.0);
                }
            lp += 0.15f * (h - lp);
            x = 0.25f * lp;
        } else if (s.material == "stabs") {
            const float T = 1.0f;
            const int k = int(t / T);
            const float nt = t - float(k) * T;
            const float env = std::exp(-nt / 0.08f);
            float h = 0.0f;
            const float notes[3] = {220.0f, 277.18f, 329.63f};
            for (int v = 0; v < 3; ++v) {
                double& p = ph[v][0];
                p += notes[v] / kFs;
                if (p > 1.0) p -= 1.0;
                h += float(2.0 * p - 1.0);
            }
            x = env * 0.5f * h;
        } else if (s.material == "pink") {
            const float w = u(rng);
            pinkB[0] = 0.99765f * pinkB[0] + w * 0.0990460f;
            pinkB[1] = 0.96300f * pinkB[1] + w * 0.2965164f;
            pinkB[2] = 0.57000f * pinkB[2] + w * 1.0526913f;
            x = 0.25f * (pinkB[0] + pinkB[1] + pinkB[2] + w * 0.1848f);
        } else if (s.material == "bass") {
            x = std::sin(twoPi * 70.0f * t);
        }
        if (stop) x *= t >= 9.0f ? 0.0f : (t <= 8.9f ? 1.0f : (9.0f - t) / 0.1f);
        L[i] = R[i] = s.level * x;
    }
    // Normalise the peak to `level` (the 'level' is the peak the input reaches).
    float pk = 1e-9f;
    for (float v : L) pk = std::max(pk, std::fabs(v));
    const float g = s.level / pk;
    for (int i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
}

// Raw knob position at block b (t seconds after the render's start).
static float rawKnob(const Sc& s, float t, float tMove)
{
    if (s.path == "hold") return s.size0;
    float u = t - tMove;
    if (u <= 0.0f) return s.size0;
    const float mv = std::max(0.0005f, s.moveMs * 0.001f), gap = s.gapMs * 0.001f;
    if (s.path == "stair") { // repeats equal steps down, each moveMs then gapMs of rest
        const float leg = mv + gap;
        int l = int(u / leg);
        if (l >= s.repeats) return s.size1;
        const float f = std::min(1.0f, (u - float(l) * leg) / mv);
        return s.size0 + (float(l) + f) / float(s.repeats) * (s.size1 - s.size0);
    }
    if (s.repeats <= 1) {
        const float f = std::min(1.0f, u / mv);
        return s.size0 + f * (s.size1 - s.size0);
    }
    // Repeated twists: big -> small -> big ...; each leg moveMs then gapMs of rest.
    const float leg = mv + gap;
    int l = int(u / leg);
    if (l >= s.repeats * 2) return s.size1; // ends small
    const float f = std::min(1.0f, (u - float(l) * leg) / mv);
    return (l % 2 == 0) ? s.size0 + f * (s.size1 - s.size0) : s.size1 + f * (s.size0 - s.size1);
}

struct Result {
    double preR50, postR50, preR1, postR1, prePk, postPk, tPk, tR50, limMin, inR50, r50final, tenv;
    long overCeil, nans, sizeSends, lastRaw;
    double finalParam, dSize, postOverPreCeilMs;
    int restarts;
};

static Result run(const Sc& s, const char* traceDir, bool dumpTrace)
{
    const float tMove = (s.material == "stabs") ? 10.0f : 10.0f;
    const float tEnd = tMove + std::max(0.0f, (s.repeats > 1 ? (s.moveMs + s.gapMs) * 0.002f * s.repeats : s.moveMs * 0.001f)) + (std::getenv("SB_POST") ? float(std::atof(std::getenv("SB_POST"))) : 5.0f);
    const int total = int(tEnd * kFs);
    std::vector<float> inL, inR;
    makeInput(s, total, inL, inR);
    auto eng = std::make_unique<iv::Engine>();
    iv::Engine& e = *eng;
    e.prepare(kFs, kBlock);
    static const int voicing = std::getenv("SB_VOICING") ? std::getenv("SB_VOICING")[0] - 'A' : 0;
    static const float capDb = std::getenv("SB_CAP_DB") ? float(std::atof(std::getenv("SB_CAP_DB"))) : (voicing == 1 ? 3.0f : 6.0f);
    e.setSizeBlastPrototype(voicing, capDb);
    using iv::ParamId;
    e.setParam(ParamId::Mix, 1.0f);
    e.setParam(ParamId::Decay, s.decay);
    e.setParam(ParamId::Tone, s.tone);
    e.setParam(ParamId::Era, s.wear);
    e.setParam(ParamId::Drift, s.motion);
    e.setParam(ParamId::Time, s.depth);
    e.setParam(ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(ParamId::Attack, iv::switchToNormalised(s.onset));
    e.setParam(ParamId::Size, s.size0);
    e.reset();

    std::vector<float> outL(total, 0.0f), outR(total, 0.0f);
    std::vector<float> limG(total / kBlock + 2, 1.0f);
    // SB_POISON "<seconds>,<tank>" (0 FDN, 1 still lows, 2 plate): a NaN into that tank's state at that time.
    float poisonAt = -1.0f; int poisonTank = 0;
    if (const char* ps = std::getenv("SB_POISON")) std::sscanf(ps, "%f,%d", &poisonAt, &poisonTank);
    float smooth = -1.0f, sent = -1.0f;
    const float kSmooth = 1.0f / (1.0f + 10.0f); // firmware: kKnobSmoothMs 10, a block = 1 ms
    long sends = 0;
    std::mt19937 knobRng(777);
    static const float jit = std::getenv("SB_JITTER") ? float(std::atof(std::getenv("SB_JITTER"))) : 0.0003f;
    std::normal_distribution<float> jitter(0.0f, jit);
    for (int pos = 0, b = 0; pos + kBlock <= total; pos += kBlock, ++b) {
        const float t = float(pos) / kFs;
        float raw = rawKnob(s, t, tMove);
        if (s.path == "knob" || s.path == "stair") {
            raw += jitter(knobRng); // 16-bit-ish ADC jitter
            raw = std::max(0.0f, std::min(1.0f, raw));
            smooth = smooth < 0.0f ? raw : smooth + kSmooth * (raw - smooth);
            const float v = ivpot::endStops(smooth);
            if (std::fabs(v - sent) > 0.003f || ((v == 0.0f || v == 1.0f) && v != sent)) {
                sent = v;
                e.setParam(ParamId::Size, v);
                if (t >= tMove) ++sends;
            }
        } else if (s.path == "ramp") {
            if (raw != sent) { e.setParam(ParamId::Size, raw); sent = raw; if (t >= tMove) ++sends; }
        }
        if (poisonAt >= 0.0f && t >= poisonAt) { e.poisonTankPrototype(poisonTank); poisonAt = -1.0f; }
        e.process(inL.data() + pos, inR.data() + pos, outL.data() + pos, outR.data() + pos, kBlock);
        limG[b] = e.limiterGain();
    }

    // Measure. Windows: pre = the 4 s before the move; post = move start to the end.
    const int tM = int(tMove * kFs);
    Result r{};
    auto frame = [&](int from, int len, const std::vector<float>& a, const std::vector<float>& c) {
        double acc = 0.0;
        for (int i = from; i < from + len && i < total; ++i) acc += 0.5 * (double(a[i]) * a[i] + double(c[i]) * c[i]);
        return std::sqrt(acc / double(len));
    };
    auto maxR = [&](int a, int z, int len, double* where) {
        double m = 0.0;
        double wh = 0.0;
        for (int i = a; i + len <= z; i += len / 2) {
            const double v = frame(i, len, outL, outR);
            if (v > m) { m = v; wh = double(i - tM) / kFs; }
        }
        if (where) *where = wh;
        return m;
    };
    const int preA = tM - int(4.0f * kFs);
    r.preR50 = maxR(preA, tM, 2400, nullptr);
    r.postR50 = maxR(tM, total, 2400, &r.tR50);
    r.preR1 = maxR(preA, tM, 48, nullptr);
    r.postR1 = maxR(tM, total, 48, nullptr);
    r.limMin = 1.0;
    double pk = 0.0;
    for (int i = preA; i < tM; ++i) pk = std::max<double>(pk, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
    r.prePk = pk;
    pk = 0.0;
    for (int i = tM; i < total; ++i) {
        const double a = std::max(std::fabs(outL[i]), std::fabs(outR[i]));
        if (a > pk) { pk = a; r.tPk = double(i - tM) / kFs; }
        if (a >= 0.88) ++r.overCeil;
        if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) ++r.nans;
    }
    r.postPk = pk;
    for (int b = tM / kBlock; b < total / kBlock; ++b) r.limMin = std::min<double>(r.limMin, limG[b]);
    r.inR50 = 0.0;
    for (int i = tM; i + 2400 <= total; i += 1200) r.inR50 = std::max(r.inR50, frame(i, 2400, inL, inR));
    r.r50final = frame(total - int(1.0f * kFs), int(1.0f * kFs), outL, outR); // the last second: the new steady level
    r.sizeSends = sends;
    r.finalParam = e.param(ParamId::Size);
    r.restarts = e.tankRestartsPrototype();
    if (const char* rawDir = std::getenv("SB_RAW")) { // float32 interleaved stereo, the restart's click check
        std::ofstream t(std::string(rawDir) + "/" + s.id + ".raw", std::ios::binary);
        for (int i = 0; i < total; ++i) { t.write((const char*)&outL[i], 4); t.write((const char*)&outR[i], 4); }
    }
    if (dumpTrace && traceDir) {
        std::string base = std::string(traceDir) + "/" + s.id;
        std::ofstream t(base + ".trace");
        for (int i = tM - 24000; i + 2400 <= total; i += 480)
            t << double(i - tM) / kFs << " " << 20.0 * std::log10(frame(i, 2400, outL, outR) + 1e-9) << "\n";
    }
#ifdef SB_PROBE
    std::fprintf(stderr,"%s: xf-line-starts %d maxLnGain %.2f (x%.1f) maxDeltaS %.3f maxRate %.4f maxK %.3f sends %ld finalSize %.3f\n",s.id.c_str(),g_xfs,g_maxLn,std::exp(g_maxLn),g_maxDelta,g_rate,g_maxK,r.sizeSends,r.finalParam);
    g_maxLn=g_maxDelta=g_rate=g_maxK=0;g_xfs=0;
#endif
    return r;
}

static double db(double x) { return 20.0 * std::log10(std::max(1e-12, x)); }

int main()
{
    std::vector<Sc> scs;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        Sc s;
        is >> s.id >> s.space >> s.onset >> s.decay >> s.tone >> s.wear >> s.motion >> s.depth >> s.size0 >> s.size1 >> s.moveMs >> s.path >> s.material >> s.level >> s.repeats >> s.gapMs;
        if (is.fail()) { std::cerr << "bad line: " << line << "\n"; return 2; }
        scs.push_back(s);
    }
    std::vector<std::string> rows(scs.size());
    std::atomic<size_t> next{0};
    const char* trace = std::getenv("SB_TRACE");
    std::string ids = std::getenv("SB_TRACE_IDS") ? std::getenv("SB_TRACE_IDS") : "";
    int nThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    if (const char* jt = std::getenv("SB_THREADS")) nThreads = std::atoi(jt);
    std::vector<std::thread> pool;
    for (int t = 0; t < nThreads; ++t)
        pool.emplace_back([&] {
            for (;;) {
                const size_t k = next.fetch_add(1);
                if (k >= scs.size()) return;
                const Sc& s = scs[k];
                const bool dump = trace && (ids.empty() || ids.find(s.id) != std::string::npos);
                const Result r = run(s, trace, dump);
                char buf[800];
                std::snprintf(buf, sizeof buf,
                              "%s,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.0f,%s,%s,%.2f,%d,%.0f,"
                              "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.2f,%ld,%ld,%ld,%.3f,%d\n",
                              s.id.c_str(), s.space, s.onset, s.decay, s.tone, s.wear, s.motion, s.depth, s.size0, s.size1, s.moveMs,
                              s.path.c_str(), s.material.c_str(), s.level, s.repeats, s.gapMs,
                              db(r.preR50), db(r.postR50), db(r.preR1), db(r.postR1), db(r.prePk), db(r.postPk), r.tPk, r.tR50, r.limMin,
                              db(r.inR50), db(r.r50final), r.overCeil, r.nans, r.sizeSends, r.finalParam, r.restarts);
                rows[k] = buf;
            }
        });
    for (auto& t : pool) t.join();
    std::cout << "id,space,onset,decay,tone,wear,motion,depth,size0,size1,moveMs,path,material,level,repeats,gapMs,"
                 "preR50,postR50,preR1,postR1,prePk,postPk,tPk,tR50,limMin,inR50,r50final,overCeil,nans,sends,finalParam,restarts\n";
    for (const auto& r : rows) std::cout << r;
    return 0;
}
