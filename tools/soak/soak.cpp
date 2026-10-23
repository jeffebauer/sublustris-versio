// Soak harness: drives iv::Engine for hours of audio and logs anything that
// breaks (docs/research/soak-audit.md). Not a ctest suite: it runs for as long
// as it's told. Every run is deterministic from its seed (the timing numbers
// aside), so a finding is reproduced by running the same seed and sample rate
// again; --dump writes the audio around a sample for listening.
//
//   iv_soak --seed 7 --rate 48000 --minutes 60 [--block N] [--log f.jsonl]
//           [--dump-at SAMPLE --dump-seconds 4 --dump out.wav]
//   iv_soak --cpu [--log cpu.jsonl]           the desktop CPU table
//
// Episodes (each a stretch of the Engine's one continuous life):
//   chaos    every knob automated (glides and jumps, switches), gestures, any stimulus
//   hold     infinite DECAY or a freeze (sometimes painted), then minutes of silence:
//            the tail must hold its level (no dying, no growth)
//   decay    a finite tail into minutes of silence: it must fall away, never rise,
//            and end in silence (no stuck noise or buzz)
//   gesture  freeze / paint / unfreeze, taps, rapid toggles, button + gate chords
//            over a steady input: no clicks, no level jumps
//   flip     SPACE flips in the middle of tails, around freezes and unfreezes
// Every sample, always: NaN / inf, the limiter's ceiling, clicks (where the
// input is smooth enough to tell), subnormal outputs; per block, the time taken.
#include "Wav.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

using iv::ParamId;
constexpr int kNumParams = int(ParamId::Count);
const char* kParamKeys[kNumParams] = {"mix", "decay", "tone", "era", "size", "drift", "time", "space", "attack"};

// ---- Random numbers (xorshift64*, seeded) ---------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull) { if (!s) s = 1; next(); next(); }
    uint32_t next()
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return uint32_t((s * 0x2545F4914F6CDD1Dull) >> 32);
    }
    float uni() { return float(next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * uni(); }
    int   irange(int a, int b) { return a + int(next() % uint32_t(b - a + 1)); }
    bool  chance(float p) { return uni() < p; }
    // A knob value: often an end, else anywhere.
    float knob(float endChance = 0.25f)
    {
        if (chance(endChance)) return chance(0.5f) ? 0.0f : 1.0f;
        return uni();
    }
    float sw() { return iv::switchToNormalised(irange(0, 2)); }
};

bool isSubnormal(float v)
{
    uint32_t u;
    std::memcpy(&u, &v, 4);
    return (u & 0x7F800000u) == 0 && (u & 0x007FFFFFu) != 0;
}

// ---- Stimulus -----------------------------------------------------------------------
enum class Stim { Silence, DC, Noise, Sine, Chord, Clicks, Burst };
const char* stimName(Stim s)
{
    switch (s) {
    case Stim::Silence: return "silence";
    case Stim::DC: return "dc";
    case Stim::Noise: return "noise";
    case Stim::Sine: return "sine";
    case Stim::Chord: return "chord";
    case Stim::Clicks: return "clicks";
    case Stim::Burst: return "burst";
    }
    return "?";
}

struct Source {
    float fs = 48000.0f;
    Stim  kind = Stim::Silence, next = Stim::Silence;
    float amp = 0.0f, nextAmp = 0.0f;
    float dcL = 0.0f, dcR = 0.0f;
    double ph[4] = {}, inc[4] = {};
    float  sineAmp[4] = {};
    uint32_t noise = 0x1234567u;
    int64_t clickGap = 4800, clickLeft = 0;
    float env = 0.0f, envStep = 0.0f;
    float burst = 0.0f, burstDecay = 0.999f;
    bool  switching = false;
    Rng*  rng = nullptr;

    // Smooth kinds ramp in and out over 20 ms; clicks and bursts start at once (impulsive).
    int64_t* changed = nullptr; // the harness's clock of the last change
    int64_t  now = 0;
    void set(Stim k, float a)
    {
        if (changed) *changed = now;
        next = k;
        nextAmp = a;
        switching = true;
        if (env <= 0.0f || kind == Stim::Silence) begin();
    }
    bool impulsive() const { return kind == Stim::Clicks || kind == Stim::Burst || (switching && (next == Stim::Clicks || next == Stim::Burst)); }
    bool smooth() const { return kind == Stim::Silence || kind == Stim::DC || kind == Stim::Sine || kind == Stim::Chord; }

    void begin()
    {
        switching = false;
        kind = next;
        amp = nextAmp;
        Rng& r = *rng;
        switch (kind) {
        case Stim::DC: dcL = amp * (r.chance(0.5f) ? 1.0f : -1.0f); dcR = r.chance(0.3f) ? -dcL : dcL; break;
        case Stim::Sine:
            inc[0] = 2.0 * M_PI * double(std::exp(r.range(std::log(20.0f), std::log(std::min(16000.0f, 0.4f * fs))))) / fs;
            sineAmp[0] = 1.0f; sineAmp[1] = sineAmp[2] = sineAmp[3] = 0.0f;
            break;
        case Stim::Chord: {
            const float root = std::exp(r.range(std::log(40.0f), std::log(800.0f)));
            const float ratios[4] = {1.0f, 1.2599f, 1.4983f, 2.0f};
            for (int i = 0; i < 4; ++i) { inc[i] = 2.0 * M_PI * double(root * ratios[i]) / fs; sineAmp[i] = 0.25f; }
            break;
        }
        case Stim::Clicks: clickGap = int64_t(r.range(0.02f, 0.8f) * fs); clickLeft = 0; break;
        case Stim::Burst: burst = 1.0f; burstDecay = std::exp(-1.0f / (r.range(0.01f, 1.5f) * fs)); break;
        default: break;
        }
        const bool ramp = kind != Stim::Clicks && kind != Stim::Burst;
        if (ramp) { env = 0.0f; envStep = 1.0f / (0.02f * fs); }
        else { env = 1.0f; envStep = 0.0f; }
    }

    float white()
    {
        noise = noise * 1664525u + 1013904223u;
        return float(int32_t(noise)) * (1.0f / 2147483648.0f);
    }

    void sample(float& l, float& r)
    {
        if (switching) {
            env -= 1.0f / (0.02f * fs);
            if (env <= 0.0f) { env = 0.0f; begin(); }
        } else if (envStep > 0.0f) {
            env += envStep;
            if (env >= 1.0f) { env = 1.0f; envStep = 0.0f; }
        }
        float a = 0.0f, b = 0.0f;
        switch (kind) {
        case Stim::Silence: break;
        case Stim::DC: a = dcL; b = dcR; break;
        case Stim::Noise: a = amp * white(); b = amp * white(); break;
        case Stim::Sine:
        case Stim::Chord:
            for (int i = 0; i < 4; ++i)
                if (sineAmp[i] > 0.0f) {
                    ph[i] += inc[i];
                    if (ph[i] > 2.0 * M_PI) ph[i] -= 2.0 * M_PI;
                    const float s = sineAmp[i] * float(std::sin(ph[i]));
                    a += s;
                    b += sineAmp[i] * float(std::sin(ph[i] + 0.5 * i));
                }
            a *= amp; b *= amp;
            break;
        case Stim::Clicks:
            if (--clickLeft <= 0) {
                clickLeft = clickGap;
                a = amp * (white() < 0.0f ? -1.0f : 1.0f);
                b = white() < -0.5f ? -a : a;
            }
            break;
        case Stim::Burst:
            a = amp * burst * white(); b = amp * burst * white();
            burst *= burstDecay;
            break;
        }
        l = env * a;
        r = env * b;
    }
};

// ---- Knob automation ------------------------------------------------------------------
struct Knob {
    float value = 0.5f, target = 0.5f, step = 0.0f;
    int64_t left = 0;
    void jump(float v) { value = target = v; left = 0; step = 0.0f; }
    void glide(float v, int64_t samples)
    {
        if (samples <= 0) { jump(v); return; }
        target = v;
        left = samples;
        step = (v - value) / float(samples);
    }
    void advance(int n)
    {
        if (left <= 0) return;
        if (left <= n) { value = target; left = 0; return; }
        value += step * float(n);
        left -= n;
    }
};

struct EdgeEv {
    int64_t at;
    bool    isButton, level;
};

// ---- The run ---------------------------------------------------------------------------
struct Finding {
    std::string type;
    int64_t     sample;
    double      value;
    std::string detail;
};

struct Soak {
    // Setup.
    uint64_t seed = 1;
    float    fs = 48000.0f;
    int      blockMode = -1; // >0 fixed size, 0 random per block, -1 pick from seed
    double   minutes = 10.0;
    FILE*    log = stdout;
    int64_t  dumpAt = -1;
    double   dumpSeconds = 4.0;
    std::string dumpPath;
    int      traceEpisode = -1;

    // State.
    Rng rng{1};
    Rng blockRng{2};
    std::unique_ptr<iv::Engine> eng;
    std::vector<float> large, fast;
    Source src;
    Knob knobs[kNumParams];
    std::vector<EdgeEv> edges; // sorted by time
    size_t edgeNext = 0;
    int64_t t = 0; // samples since start
    int episode = 0;
    const char* episodeKind = "";
    bool stillLows = true;

    // Monitors.
    bool  clickArmed = true;      // the episode allows click checks
    int64_t lastImpulsive = -1000000000;
    int64_t lastKnobJump = -1000000000;
    int64_t lastStimChange = -1000000000;
    int64_t lastEdge = -1000000000, lastFlip = -1000000000, lastFreezeChange = -1000000000;
    int     lastSpaceSeen = -1;
    // How long since each kind of event, for triaging a finding (ms).
    std::string context(int64_t at) const
    {
        char b[160];
        auto ms = [&](int64_t since) { return since < -100000000 ? -1.0 : 1000.0 * double(at - since) / fs; };
        std::snprintf(b, sizeof b, "since edge %.0f ms, flip %.0f ms, freeze/unfreeze %.0f ms, input change %.0f ms, knob jump %.0f ms", ms(lastEdge),
                      ms(lastFlip), ms(lastFreezeChange), ms(lastStimChange), ms(lastKnobJump));
        return b;
    }
    float y1[2] = {}, y2[2] = {}, x1[2] = {}, x2[2] = {}, dp[2] = {};
    float dpCoeff = 0.0f;
    int64_t subnormalOut = 0;
    // Window power (100 ms) for the episode's level analysis.
    std::vector<float> winDb;
    double winAcc = 0.0;
    int    winFill = 0, winLen = 4800;
    int64_t episodeStart = 0;
    // Gesture level jumps: the last 4096 samples' power, and checks waiting for their post window.
    std::vector<float> powRing;
    struct PendingJump { int64_t at; double pre; std::string what; };
    std::vector<PendingJump> pendingJumps;
    bool lastFrozen = false, lastPainting = false;
    bool jumpArmed = false;
    // Timing.
    double  nsBaseline[3] = {0, 0, 0};
    int64_t blocksTimed[3] = {0, 0, 0};
    int64_t spikes = 0, slowSeconds = 0;
    double  nsTotal[3] = {0, 0, 0};
    int64_t samplesBySpace[3] = {0, 0, 0};
    double  winNs = 0.0; int64_t winNsSamples = 0;
    // Findings.
    int64_t counts[32] = {};
    std::vector<std::string> typeNames;
    int perEpisodeCount[32] = {};
    int64_t totalFindings = 0;
    // Dump.
    iv::wav::Audio dump;

    int typeIndex(const std::string& s)
    {
        for (size_t i = 0; i < typeNames.size(); ++i)
            if (typeNames[i] == s) return int(i);
        typeNames.push_back(s);
        return int(typeNames.size() - 1);
    }

    std::string settings() const
    {
        std::string s = "{";
        char b[64];
        for (int i = 0; i < kNumParams; ++i) {
            std::snprintf(b, sizeof b, "%s\"%s\":%.4f", i ? "," : "", kParamKeys[i], eng->param(ParamId(i)));
            s += b;
        }
        std::snprintf(b, sizeof b, ",\"frozen\":%d,\"painting\":%d,\"space_now\":%d}", int(eng->frozen()), int(eng->painting()),
                      eng->activeSpace());
        return s + b;
    }

    void report(const std::string& type, int64_t at, double value, const std::string& detail)
    {
        const int ti = typeIndex(type);
        ++counts[ti];
        ++totalFindings;
        if (++perEpisodeCount[ti] > 3) return; // the first few per episode; the rest are counted
        std::fprintf(log,
                     "{\"kind\":\"finding\",\"type\":\"%s\",\"seed\":%llu,\"rate\":%d,\"block\":%d,\"episode\":%d,\"episode_kind\":\"%s\","
                     "\"sample\":%lld,\"time_s\":%.4f,\"value\":%.6g,\"stim\":\"%s\",\"detail\":\"%s\",\"settings\":%s,"
                     "\"repro\":\"iv_soak --seed %llu --rate %d%s --minutes %.1f --dump-at %lld --dump out.wav\"}\n",
                     type.c_str(), (unsigned long long)seed, int(fs), blockMode, episode, episodeKind, (long long)at, double(at) / fs, value,
                     stimName(src.kind), detail.c_str(), settings().c_str(), (unsigned long long)seed, int(fs),
                     blockMode > 0 ? (" --block " + std::to_string(blockMode)).c_str() : "", minutes, (long long)at);
        std::fflush(log);
    }
    void stat(const std::string& type, const std::string& json)
    {
        std::fprintf(log, "{\"kind\":\"stat\",\"type\":\"%s\",\"seed\":%llu,\"rate\":%d,\"episode\":%d,\"settings\":%s,%s}\n", type.c_str(),
                     (unsigned long long)seed, int(fs), episode, settings().c_str(), json.c_str());
        std::fflush(log);
    }

    void prepareEngine()
    {
        eng = std::make_unique<iv::Engine>();
        large.assign(iv::Engine::largeFloats(fs), 0.0f);
        fast.assign(iv::Engine::fastFloats(fs), 0.0f);
        eng->attachMemory(large.data(), large.size(), fast.data(), fast.size());
        eng->prepare(fs, 256);
        eng->setStillLows(stillLows);
        for (int i = 0; i < kNumParams; ++i) knobs[i].jump(iv::kParams[size_t(i)].defaultValue);
        if (!eng->ready()) report("not_ready", t, 0, "Engine::ready() false after prepare");
    }

    void init()
    {
        rng = Rng(seed);
        blockRng = Rng(seed ^ 0xB10C);
        {
            // Always drawn, so --block (which overrides it) leaves the rest of the run unchanged.
            static const int sizes[] = {1, 2, 3, 7, 16, 31, 32, 33, 47, 48, 64, 100, 127, 128, 200, 255, 256};
            const int pick = int(rng.next() % 20u);
            if (blockMode < 0) blockMode = pick < 17 ? sizes[pick] : 0; // 0 = random every block
        }
        src.fs = fs;
        src.rng = &rng;
        src.changed = &lastStimChange;
        dpCoeff = 1.0f - std::exp(-1.0f / (0.01f * fs));
        winLen = int(0.1f * fs);
        powRing.assign(4096, 0.0f);
        prepareEngine();
        if (dumpAt >= 0) {
            dump.sampleRate = int(fs);
            dump.isFloat = true;
            dump.channels.assign(4, {});
        }
    }

    // ---- The per-sample monitor -----------------------------------------------------------
    void monitor(const float* inL, const float* inR, const float* outL, const float* outR, int n)
    {
        const float* in[2] = {inL, inR};
        const float* out[2] = {outL, outR};
        // Not where clicks are the design: ERA's Midiverb crackle (ERA past 0.85, SPEC §4.7), and the
        // limiter's instant attack on a pad loud enough to reach its ceiling (SPEC §4.10).
        const bool designedClicks = eng->param(ParamId::Era) > 0.85f || eng->limiterGain() < 0.98f;
        const bool clickOk = clickArmed && src.smooth() && !src.switching && t - lastImpulsive > int64_t(8.0f * fs)
                          && t - lastKnobJump > int64_t(0.5f * fs) && !designedClicks;
        for (int i = 0; i < n; ++i) {
            const int64_t at = t + i;
            double p = 0.0;
            for (int c = 0; c < 2; ++c) {
                const float y = out[c][i], x = in[c][i];
                if (!std::isfinite(y)) {
                    report("nan_inf", at, 0, c ? "right" : "left");
                    return; // the caller re-prepares
                }
                const float bound = std::sqrt(x * x + 0.8911f * 0.8911f) + 1e-5f;
                if (std::fabs(y) > bound) {
                    char d[96];
                    std::snprintf(d, sizeof d, "|out| %.5f over bound %.5f (in %.4f)", std::fabs(y), bound, x);
                    report("over_ceiling", at, std::fabs(y), d);
                }
                if (isSubnormal(y)) ++subnormalOut;
                const float d2 = y - 2.0f * y1[c] + y2[c];
                const float xd2 = x - 2.0f * x1[c] + x2[c];
                if (clickOk) {
                    const float local = std::sqrt(dp[c]);
                    const float thr = 10.0f * local + 1.5f * std::fabs(xd2) + 0.004f;
                    if (std::fabs(d2) > thr) {
                        char d[128];
                        std::snprintf(d, sizeof d, "jump %.5f vs local %.5f (x%.1f), %s", std::fabs(d2), local, std::fabs(d2) / (local + 1e-9f),
                                      c ? "right" : "left");
                        report("click", at, std::fabs(d2) / (local + 1e-9f), std::string(d) + "; " + context(at));
                    }
                }
                dp[c] += dpCoeff * (d2 * d2 - dp[c]);
                y2[c] = y1[c]; y1[c] = y;
                x2[c] = x1[c]; x1[c] = x;
                p += 0.5 * double(y) * y;
            }
            powRing[size_t(at) & 4095] = float(p);
            winAcc += p;
            if (++winFill == winLen) {
                winDb.push_back(float(10.0 * std::log10(winAcc / winLen + 1e-30)));
                winAcc = 0.0;
                winFill = 0;
            }
        }
    }

    double ringDb(int64_t from, int64_t to) const // mean power over [from, to), within the last 4096 samples
    {
        double acc = 0.0;
        for (int64_t s = from; s < to; ++s) acc += powRing[size_t(s) & 4095];
        return 10.0 * std::log10(acc / double(std::max<int64_t>(1, to - from)) + 1e-30);
    }

    // ---- Processing ------------------------------------------------------------------------
    std::vector<float> bufInL = std::vector<float>(256), bufInR = std::vector<float>(256), bufOutL = std::vector<float>(256),
                       bufOutR = std::vector<float>(256);

    int nextBlock()
    {
        if (blockMode > 0) return blockMode;
        return blockRng.irange(1, 256);
    }

    void addEdge(int64_t at, bool isButton, bool level)
    {
        EdgeEv e{at, isButton, level};
        auto it = std::upper_bound(edges.begin() + long(edgeNext), edges.end(), e, [](const EdgeEv& a, const EdgeEv& b) { return a.at < b.at; });
        edges.insert(it, e);
    }
    void press(int64_t at, double seconds, bool isButton = true)
    {
        addEdge(at, isButton, true);
        addEdge(at + std::max<int64_t>(1, int64_t(seconds * fs)), isButton, false);
    }
    void clearEdges()
    {
        // Release anything held at the end of an episode (no stuck presses into the next).
        edges.erase(edges.begin() + long(edgeNext), edges.end());
        if (eng->buttonDown()) addEdge(t, true, false);
        if (eng->gateHigh()) addEdge(t, false, false);
    }

    using Hook = void (*)(Soak&);
    // Runs `seconds` of audio. perBlock (optional) runs before each block (automation programs).
    template <typename F>
    void run(double seconds, F perBlock)
    {
        const int64_t end = t + int64_t(seconds * fs);
        while (t < end) {
            int n = nextBlock();
            if (t + n > end) n = int(end - t);
            perBlock(n);
            for (int i = 0; i < kNumParams; ++i) {
                knobs[i].advance(n);
                eng->setParam(ParamId(i), knobs[i].value);
            }
            int edgesThis = 0;
            while (edgeNext < edges.size() && edges[edgeNext].at < t + n) {
                const EdgeEv& e = edges[edgeNext++];
                const int off = int(std::max<int64_t>(0, e.at - t));
                if (++edgesThis <= 16) {
                    lastEdge = e.at;
                    if (e.isButton) eng->button(e.level, off);
                    else eng->gate(e.level, off);
                }
            }
            src.now = t;
            for (int i = 0; i < n; ++i) {
                src.sample(bufInL[size_t(i)], bufInR[size_t(i)]);
                if (src.impulsive()) lastImpulsive = t + i;
            }
            const auto t0 = std::chrono::steady_clock::now();
            eng->process(bufInL.data(), bufInR.data(), bufOutL.data(), bufOutR.data(), n);
            const auto t1 = std::chrono::steady_clock::now();
            const double ns = double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            timing(ns, n);
            const int64_t before = totalFindings;
            monitor(bufInL.data(), bufInR.data(), bufOutL.data(), bufOutR.data(), n);
            if (dumpAt >= 0) {
                const int64_t from = dumpAt - int64_t(dumpSeconds * 0.5 * fs), to = dumpAt + int64_t(dumpSeconds * 0.5 * fs);
                for (int i = 0; i < n; ++i)
                    if (t + i >= from && t + i < to) {
                        dump.channels[0].push_back(bufInL[size_t(i)]);
                        dump.channels[1].push_back(bufInR[size_t(i)]);
                        dump.channels[2].push_back(bufOutL[size_t(i)]);
                        dump.channels[3].push_back(bufOutR[size_t(i)]);
                    }
            }
            gestureWatch(n);
            if (traceEpisode == episode && (t + n) / int64_t(fs) != t / int64_t(fs)) {
                // --trace-episode: each second, what the episode is doing (for reproducing a finding)
                std::fprintf(stderr, "t %8.1f s  ep %d %-8s  out %6.1f dB  tank %6.1f slow %6.1f  in %s %.3f  frozen %d painting %d space %d\n",
                             double(t + n) / fs, episode, episodeKind, winDb.empty() ? -300.0 : double(winDb.back()), eng->tankLevelDb(),
                             eng->tankSlowLevelDb(), stimName(src.kind), src.amp, int(eng->frozen()), int(eng->painting()), eng->activeSpace());
            }
            t += n;
            (void)before;
            for (int i = 0; i < n; ++i) // poisoned by a NaN: start over
                if (!std::isfinite(bufOutL[size_t(i)]) || !std::isfinite(bufOutR[size_t(i)])) { prepareEngine(); break; }
            if ((t / int64_t(fs)) % 10 == 0 && (t - n) / int64_t(fs) % 10 != 0) scanMemory();
        }
    }
    void run(double seconds) { run(seconds, [](int) {}); }

    void timing(double ns, int n)
    {
        const int sp = eng->activeSpace();
        nsTotal[sp] += ns;
        samplesBySpace[sp] += n;
        const double perSample = ns / n;
        if (n >= 32) {
            double& base = nsBaseline[sp];
            if (blocksTimed[sp]++ < 50) base = blocksTimed[sp] == 1 ? perSample : std::min(base * 1.05, base + (perSample - base) * 0.1);
            else if (perSample < base * 4.0) base += (perSample - base) * 0.002;
            if (blocksTimed[sp] > 200 && perSample > 30.0 * base && ns > 200000.0) ++spikes;
        }
        // A sustained slowdown: a whole second at 3x the space's baseline (denormals would show as this).
        winNs += ns;
        winNsSamples += n;
        if (winNsSamples >= int64_t(fs)) {
            const double avg = winNs / double(winNsSamples);
            // Counted, not reported: parallel runs share the CPU (and an efficiency core is 3x slower),
            // so a slow second is mostly the machine. The CPU table (--cpu) measures alone.
            if (blocksTimed[sp] > 500 && avg > 3.0 * nsBaseline[sp] && nsBaseline[sp] > 0.0) ++slowSeconds;
            winNs = 0.0;
            winNsSamples = 0;
        }
    }

    // Subnormal floats sitting in the delay memory (slow on x86 desktops without flush-to-zero).
    int64_t subnormalMemMax = 0;
    bool    subnormalReported = false;
    void scanMemory()
    {
        int64_t n = 0;
        for (float v : large) n += isSubnormal(v);
        for (float v : fast) n += isSubnormal(v);
        if (n > subnormalMemMax) subnormalMemMax = n;
        if (n > 0 && n * 1000 > int64_t(large.size()) && !subnormalReported) { // more than 0.1 % of the memory; once per run
            subnormalReported = true;
            char d[96];
            std::snprintf(d, sizeof d, "%lld subnormal floats in delay memory (%.2f %%)", (long long)n, 100.0 * double(n) / double(large.size() + fast.size()));
            report("subnormal_memory", t, double(n), d);
        }
    }

    void gestureWatch(int n)
    {
        const bool fr = eng->frozen(), pa = eng->painting();
        if (fr != lastFrozen) lastFreezeChange = t;
        if (eng->activeSpace() != lastSpaceSeen) { lastFlip = t; lastSpaceSeen = eng->activeSpace(); }
        if (jumpArmed && (fr != lastFrozen || pa != lastPainting) && t - lastStimChange > int64_t(0.2f * fs)) {
            const char* what = fr != lastFrozen ? (fr ? "freeze" : "unfreeze") : (pa ? "paint start" : "paint stop");
            // The event happened somewhere in this block: compare 10 ms before the block with 10 ms after it.
            const int64_t w = int64_t(0.01f * fs);
            pendingJumps.push_back({t + n, ringDb(t - w, t), what});
        }
        lastFrozen = fr;
        lastPainting = pa;
        for (size_t i = 0; i < pendingJumps.size();) {
            const int64_t w = int64_t(0.01f * fs);
            if (t + n >= pendingJumps[i].at + w) {
                const double post = ringDb(pendingJumps[i].at, pendingJumps[i].at + w), pre = pendingJumps[i].pre;
                const double diff = post - pre;
                const bool reverse = iv::normalisedToSwitch(eng->param(ParamId::Attack)) == 2;
                const double upLimit = src.kind == Stim::Silence ? 4.0 : 6.0;
                if (!reverse && ((diff > upLimit && post > -50.0) || (diff < -10.0 && pre > -50.0))) {
                    char d[128];
                    std::snprintf(d, sizeof d, "%s: %.1f dB -> %.1f dB in 10 ms", pendingJumps[i].what.c_str(), pre, post);
                    report("gesture_level_jump", pendingJumps[i].at, diff, d);
                }
                pendingJumps.erase(pendingJumps.begin() + long(i));
            } else ++i;
        }
    }

    // ---- Episode helpers ---------------------------------------------------------------------
    // A finished episode may leave the big tank frozen (chaos does): tap it free first.
    void unfreeze()
    {
        clearEdges();
        run(0.02);
        if (eng->frozen()) {
            press(t + 1, 0.05);
            run(1.5);
        }
    }
    void beginEpisode(const char* kind)
    {
        if (std::strcmp(kind, "chaos") != 0 && eng) unfreeze();
        ++episode;
        episodeKind = kind;
        std::memset(perEpisodeCount, 0, sizeof perEpisodeCount);
        winDb.clear();
        winAcc = 0.0;
        winFill = 0;
        episodeStart = t;
        clickArmed = true;
        jumpArmed = false;
        pendingJumps.clear();
    }
    void setKnob(ParamId id, float v, bool glide = false)
    {
        Knob& k = knobs[int(id)];
        if (glide) k.glide(v, int64_t(rng.range(0.005f, 0.3f) * fs));
        else {
            if (std::fabs(k.value - v) > 0.05f) lastKnobJump = t;
            k.jump(v);
        }
    }
    void staticSettings(bool extremes)
    {
        setKnob(ParamId::Tone, rng.knob());
        setKnob(ParamId::Era, extremes && rng.chance(0.5f) ? 1.0f : rng.knob());
        setKnob(ParamId::Size, extremes ? (rng.chance(0.35f) ? 0.0f : (rng.chance(0.55f) ? 1.0f : rng.uni())) : rng.knob());
        setKnob(ParamId::Drift, extremes ? (rng.chance(0.35f) ? 0.0f : (rng.chance(0.55f) ? 1.0f : rng.uni())) : rng.knob());
        setKnob(ParamId::Time, rng.knob());
        setKnob(ParamId::Attack, rng.sw());
        setKnob(ParamId::Space, rng.sw());
    }
    // Windows (100 ms) in dB from `fromS` seconds into the episode, smoothed over `avgS`.
    std::vector<float> smoothed(double fromS, double avgS) const
    {
        std::vector<float> out;
        const int from = int(fromS * 10.0), w = std::max(1, int(avgS * 10.0));
        for (int i = from; i + w <= int(winDb.size()); i += 1) {
            double acc = 0.0;
            for (int j = 0; j < w; ++j) acc += std::pow(10.0, winDb[size_t(i + j)] / 10.0);
            out.push_back(float(10.0 * std::log10(acc / w + 1e-30)));
        }
        return out;
    }
    double episodeSeconds() const { return double(t - episodeStart) / fs; }
    void playInput(double seconds, bool allowImpulsive)
    {
        Stim kinds[] = {Stim::Noise, Stim::Chord, Stim::Sine, Stim::Burst, Stim::DC, Stim::Clicks};
        const int nk = allowImpulsive ? 6 : 3;
        src.set(kinds[rng.irange(0, nk - 1)], rng.chance(0.2f) ? 1.0f : rng.range(0.05f, 0.8f));
        run(seconds);
        src.set(Stim::Silence, 0.0f);
        run(0.03);
    }

    // ---- Episodes ---------------------------------------------------------------------------------
    void chaos()
    {
        beginEpisode("chaos");
        const double len = rng.range(20.0f, 90.0f);
        int64_t nextKnob[kNumParams], nextStim = t, nextGesture = t + int64_t(rng.range(0.5f, 5.0f) * fs);
        for (auto& k : nextKnob) k = t + int64_t(rng.range(0.0f, 3.0f) * fs);
        run(len, [&](int) {
            for (int i = 0; i < kNumParams; ++i) {
                if (t < nextKnob[i]) continue;
                const bool isSwitch = iv::kParams[size_t(i)].kind == iv::ParamKind::Switch3;
                if (isSwitch) {
                    setKnob(ParamId(i), rng.sw());
                    nextKnob[i] = t + int64_t(rng.range(0.5f, 20.0f) * fs);
                } else {
                    if (rng.chance(0.3f)) setKnob(ParamId(i), rng.knob());
                    else knobs[i].glide(rng.knob(), int64_t(rng.range(0.005f, 8.0f) * fs));
                    nextKnob[i] = t + int64_t(rng.range(0.1f, 10.0f) * fs);
                }
            }
            if (t >= nextStim) {
                Stim kinds[] = {Stim::Silence, Stim::DC, Stim::Noise, Stim::Sine, Stim::Chord, Stim::Clicks, Stim::Burst};
                src.set(kinds[rng.irange(0, 6)], rng.chance(0.2f) ? 1.0f : rng.range(0.01f, 0.9f));
                nextStim = t + int64_t(rng.range(0.3f, 10.0f) * fs);
            }
            if (t >= nextGesture) {
                const bool isButton = rng.chance(0.7f);
                const float r = rng.uni();
                const double hold = r < 0.5f ? rng.range(0.003f, 0.25f) : (r < 0.8f ? rng.range(0.35f, 6.0f) : rng.range(0.25f, 0.35f));
                press(t + rng.irange(0, 255), hold, isButton);
                if (rng.chance(0.15f)) press(t + int64_t(rng.range(0.0f, 0.5f) * fs), rng.range(0.01f, 2.0f), !isButton); // chord
                nextGesture = t + int64_t((hold + rng.range(0.05f, 12.0f)) * fs);
            }
        });
        clearEdges();
        run(0.05);
    }

    void hold()
    {
        beginEpisode("hold");
        const bool freeze = rng.chance(0.5f);
        const bool paint = freeze && rng.chance(0.3f);
        const bool extremes = rng.chance(0.5f);
        staticSettings(extremes);
        setKnob(ParamId::Mix, 1.0f);
        setKnob(ParamId::Decay, freeze ? rng.knob() : (rng.chance(0.4f) ? 1.0f : rng.range(0.85f, 1.0f)));
        src.set(Stim::Silence, 0.0f);
        run(0.5); // settle (a SPACE flip)
        const double inS = rng.range(2.0f, 8.0f);
        Stim kinds[] = {Stim::Noise, Stim::Chord, Stim::Sine, Stim::Burst, Stim::DC};
        src.set(kinds[rng.irange(0, 4)], rng.chance(0.2f) ? 1.0f : rng.range(0.05f, 0.8f));
        double refS;
        if (freeze && rng.chance(0.5f)) { // freeze while playing, then keep playing into the twin a while
            run(inS);
            press(t + 3, 0.05);
            run(rng.range(0.1f, 4.0f));
            refS = episodeSeconds() + 25.0;
            src.set(Stim::Silence, 0.0f);
        } else {
            run(inS);
            src.set(Stim::Silence, 0.0f);
            run(0.05);
            if (freeze) press(t + 3, 0.05);
            refS = episodeSeconds() + 25.0;
        }
        if (paint) {
            run(30.0);
            src.set(Stim::Chord, rng.range(0.1f, 0.7f));
            run(0.3);
            press(t + 3, rng.range(1.0f, 5.0f)); // held past the tap window: paint
            run(6.0);
            src.set(Stim::Silence, 0.0f);
            refS = episodeSeconds() + 25.0;
        }
        jumpArmed = false;
        const double holdS = rng.range(150.0f, 330.0f);
        run(holdS);
        // The tail must hold: from refS on, the 4 s level stays within a window of its value at refS.
        const auto lv = smoothed(refS, 4.0);
        if (lv.size() > 10) {
            const float ref = lv.front();
            float hi = ref, lo = ref;
            for (float v : lv) { hi = std::max(hi, v); lo = std::min(lo, v); }
            const float end = lv.back();
            char j[256];
            std::snprintf(j, sizeof j, "\"freeze\":%d,\"paint\":%d,\"extremes\":%d,\"ref_db\":%.2f,\"max_up_db\":%.2f,\"max_down_db\":%.2f,\"end_db\":%.2f,\"span_s\":%.1f",
                          int(freeze), int(paint), int(extremes), ref, hi - ref, ref - lo, end - ref, double(lv.size()) / 10.0);
            std::string series = std::string(j) + ",\"series_2s\":[";
            for (size_t i = 0; i < lv.size(); i += 20) series += (i ? "," : "") + std::to_string(int(std::lround((lv[i] - ref) * 10.0f)) / 10.0f).substr(0, 5);
            stat("hold", series + "]");
            if (ref > -90.0f && ref - lo > 3.0f) {
                char d[128];
                std::snprintf(d, sizeof d, "%s tail fell %.1f dB below its settled %.1f dB", freeze ? "frozen" : "infinite", ref - lo, ref);
                report("hold_dies", t, ref - lo, d);
            }
            if (ref > -90.0f && hi - ref > 2.0f) {
                char d[128];
                std::snprintf(d, sizeof d, "%s tail grew %.1f dB above its settled %.1f dB", freeze ? "frozen" : "infinite", hi - ref, ref);
                report("hold_grows", t, hi - ref, d);
            }
        }
        if (freeze) { // unfreeze (a tap) and listen to the glide back
            jumpArmed = true;
            press(t + 3, 0.05);
            run(3.0);
        }
        clearEdges();
        run(0.05);
    }

    void decay()
    {
        beginEpisode("decay");
        staticSettings(rng.chance(0.4f));
        setKnob(ParamId::Mix, 1.0f);
        const float d = rng.chance(0.2f) ? (rng.chance(0.5f) ? 0.0f : 0.8f) : rng.range(0.0f, 0.8f);
        setKnob(ParamId::Decay, d);
        run(0.5);
        playInput(rng.range(0.3f, 5.0f), true);
        const int space = iv::normalisedToSwitch(knobs[int(ParamId::Space)].value);
        float t60 = 1.0f / iv::map::decayRate(d, space);
        if (space == 1) t60 *= iv::map::hallT60Scale(knobs[int(ParamId::Size)].value);
        if (space == 0) t60 = std::max(t60, 0.6f);
        const double silenceS = std::min(330.0, std::max(30.0, 3.0 * t60 + 15.0));
        const double from = episodeSeconds();
        run(silenceS);
        const auto lv = smoothed(from, 1.0);
        float runMin = 1e9f, worstRise = 0.0f;
        int64_t riseAt = 0;
        for (size_t i = 0; i < lv.size(); ++i) {
            if (double(i) / 10.0 < 12.0) continue; // BLOOM, predelay, MASSIVE's swell: let it build first
            runMin = std::min(runMin, lv[i]);
            if (lv[i] > -80.0f && lv[i] - runMin > worstRise) { worstRise = lv[i] - runMin; riseAt = int64_t(i); }
        }
        const float endDb = lv.empty() ? -300.0f : lv.back();
        const bool expectSilent = silenceS > 2.8 * t60 + 12.0;
        char j[200];
        std::snprintf(j, sizeof j, "\"t60_s\":%.2f,\"silence_s\":%.1f,\"end_db\":%.1f,\"worst_rise_db\":%.2f,\"expect_silent\":%d", t60, silenceS, endDb,
                      worstRise, int(expectSilent));
        stat("decay", j);
        if (worstRise > 6.0f) {
            char dd[128];
            std::snprintf(dd, sizeof dd, "finite tail rose %.1f dB above its running low, %.1f s into silence", worstRise, double(riseAt) / 10.0);
            report("decay_rises", t, worstRise, dd);
        }
        if (expectSilent && endDb > -120.0f) {
            char dd[128];
            std::snprintf(dd, sizeof dd, "finite tail still at %.1f dBFS after %.0f s of silence (T60 %.1f s)", endDb, silenceS, t60);
            report("stuck_tail", t, endDb, dd);
        }
    }

    void gesture()
    {
        beginEpisode("gesture");
        staticSettings(rng.chance(0.3f));
        setKnob(ParamId::Mix, rng.chance(0.4f) ? 1.0f : rng.knob());
        setKnob(ParamId::Decay, rng.knob());
        run(0.5);
        // A sound to freeze, then a steady input (or silence) while the gestures play.
        playInput(rng.range(1.0f, 4.0f), false);
        Stim steady[] = {Stim::Silence, Stim::Silence, Stim::Sine, Stim::Chord, Stim::DC, Stim::Noise};
        src.set(steady[rng.irange(0, 5)], rng.range(0.05f, 0.7f));
        run(0.1);
        jumpArmed = true;
        const double len = rng.range(30.0f, 90.0f);
        int64_t at = t + int64_t(0.2f * fs);
        const int64_t end = t + int64_t(len * fs);
        while (at < end) {
            const float r = rng.uni();
            if (r < 0.35f) { press(at, rng.range(0.005f, 0.25f), rng.chance(0.7f)); at += int64_t(rng.range(0.3f, 6.0f) * fs); }
            else if (r < 0.55f) { const double h = rng.range(0.4f, 6.0f); press(at, h, rng.chance(0.7f)); at += int64_t((h + rng.range(0.2f, 4.0f)) * fs); }
            else if (r < 0.7f) { // rapid toggles
                const int k = rng.irange(2, 20);
                for (int i = 0; i < k; ++i) { press(at, rng.range(0.002f, 0.03f), rng.chance(0.5f)); at += int64_t(rng.range(0.005f, 0.05f) * fs); }
                at += int64_t(rng.range(0.5f, 3.0f) * fs);
            } else if (r < 0.85f) { // button + gate chord: overlapping presses
                press(at, rng.range(0.05f, 3.0f), true);
                press(at + int64_t(rng.range(0.0f, 0.5f) * fs), rng.range(0.02f, 3.0f), false);
                at += int64_t(rng.range(1.0f, 5.0f) * fs);
            } else { // double tap
                press(at, 0.04, true);
                press(at + int64_t(rng.range(0.06f, 0.3f) * fs), 0.04, true);
                at += int64_t(rng.range(0.5f, 4.0f) * fs);
            }
        }
        run(len + 0.5);
        clearEdges();
        run(0.3);
        if (eng->frozen()) { press(t + 1, 0.05); run(2.0); }
    }

    void flip()
    {
        beginEpisode("flip");
        staticSettings(rng.chance(0.3f));
        setKnob(ParamId::Mix, rng.chance(0.5f) ? 1.0f : rng.knob());
        setKnob(ParamId::Decay, rng.range(0.6f, 1.0f));
        run(0.5);
        playInput(rng.range(2.0f, 5.0f), false);
        if (rng.chance(0.3f)) src.set(Stim::Sine, rng.range(0.05f, 0.5f));
        jumpArmed = true;
        const double len = rng.range(40.0f, 90.0f);
        int64_t nextAct = t + int64_t(0.3f * fs);
        int64_t flipAt = -1;
        int pendingSpace = -1;
        run(len, [&](int) {
            if (flipAt >= 0 && t >= flipAt) {
                setKnob(ParamId::Space, iv::switchToNormalised(pendingSpace));
                flipAt = -1;
            }
            if (t < nextAct) return;
            const float r = rng.uni();
            const int cur = iv::normalisedToSwitch(knobs[int(ParamId::Space)].value);
            const int other = (cur + rng.irange(1, 2)) % 3;
            if (r < 0.35f) { // a plain flip
                setKnob(ParamId::Space, iv::switchToNormalised(other));
            } else if (r < 0.5f) { // two flips in quick succession (during the fade)
                setKnob(ParamId::Space, iv::switchToNormalised(other));
                pendingSpace = (other + rng.irange(1, 2)) % 3;
                flipAt = t + int64_t(rng.range(0.005f, 0.08f) * fs);
            } else if (r < 0.7f) { // freeze, play a little into the twin, unfreeze, flip soon after
                press(t + 1, 0.05);
                if (!rng.chance(0.4f)) src.set(Stim::Chord, rng.range(0.05f, 0.6f));
                const double heldS = rng.range(0.5f, 4.0f);
                press(t + int64_t(heldS * fs), 0.05);
                pendingSpace = other;
                flipAt = t + int64_t((heldS + rng.range(0.0f, 2.5f)) * fs);
            } else if (r < 0.85f) { // flip while frozen (waits for the unfreeze)
                press(t + 1, 0.05);
                pendingSpace = other;
                flipAt = t + int64_t(rng.range(0.1f, 1.0f) * fs);
                press(t + int64_t(rng.range(1.5f, 4.0f) * fs), 0.05);
            } else { // freeze right as a flip starts (inside its fade)
                setKnob(ParamId::Space, iv::switchToNormalised(other));
                press(t + int64_t(rng.range(0.0f, 0.06f) * fs), 0.05);
                press(t + int64_t(rng.range(1.0f, 3.0f) * fs), 0.05);
            }
            if (rng.chance(0.3f)) src.set(Stim::Silence, 0.0f);
            nextAct = t + int64_t(rng.range(0.3f, 6.0f) * fs);
        });
        clearEdges();
        run(0.3);
        if (eng->frozen()) { press(t + 1, 0.05); run(2.0); }
    }

    void soak()
    {
        init();
        std::fprintf(log, "{\"kind\":\"start\",\"seed\":%llu,\"rate\":%d,\"block\":%d,\"minutes\":%.1f}\n", (unsigned long long)seed, int(fs), blockMode,
                     minutes);
        const auto wall0 = std::chrono::steady_clock::now();
        const int64_t total = int64_t(minutes * 60.0 * fs);
        int kinds[5] = {};
        while (t < total) {
            if (rng.chance(0.03f)) eng->reset(); // a host's reset in the middle of things
            const float r = rng.uni();
            if (r < 0.30f) { chaos(); ++kinds[0]; }
            else if (r < 0.50f) { hold(); ++kinds[1]; }
            else if (r < 0.65f) { decay(); ++kinds[2]; }
            else if (r < 0.82f) { gesture(); ++kinds[3]; }
            else { flip(); ++kinds[4]; }
            if (dumpAt >= 0 && t > dumpAt + int64_t(dumpSeconds * fs)) break;
        }
        const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
        std::string types = "{";
        for (size_t i = 0; i < typeNames.size(); ++i)
            types += (i ? ",\"" : "\"") + typeNames[i] + "\":" + std::to_string(counts[i]);
        types += "}";
        std::fprintf(log,
                     "{\"kind\":\"end\",\"seed\":%llu,\"rate\":%d,\"block\":%d,\"audio_s\":%.1f,\"wall_s\":%.1f,\"episodes\":%d,"
                     "\"chaos\":%d,\"hold\":%d,\"decay\":%d,\"gesture\":%d,\"flip\":%d,\"findings\":%s,\"timing_spikes\":%lld,\"slow_seconds\":%lld,"
                     "\"subnormal_out\":%lld,\"subnormal_mem_max\":%lld,\"ns_per_sample\":[%.1f,%.1f,%.1f]}\n",
                     (unsigned long long)seed, int(fs), blockMode, double(t) / fs, wall, episode, kinds[0], kinds[1], kinds[2], kinds[3], kinds[4],
                     types.c_str(), (long long)spikes, (long long)slowSeconds, (long long)subnormalOut, (long long)subnormalMemMax,
                     samplesBySpace[0] ? nsTotal[0] / double(samplesBySpace[0]) : 0.0, samplesBySpace[1] ? nsTotal[1] / double(samplesBySpace[1]) : 0.0,
                     samplesBySpace[2] ? nsTotal[2] / double(samplesBySpace[2]) : 0.0);
        if (dumpAt >= 0) {
            std::string err;
            if (!iv::wav::write(dumpPath, dump, err)) std::fprintf(stderr, "%s\n", err.c_str());
            else std::fprintf(stderr, "wrote %s (in L, in R, out L, out R)\n", dumpPath.c_str());
        }
    }
};

// ---- CPU table ----------------------------------------------------------------------------------
struct CpuCase {
    const char* name;
    int space;
    float decay, era, drift, time, attack;
    bool stillLows, freeze, silentTail;
};

void cpuTable(FILE* log)
{
    const float fs = 48000.0f;
    const int block = 48;
    const CpuCase cases[] = {
        {"PLATE, defaults", 0, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, false, false},
        {"HALL, defaults (still lows on)", 1, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, false, false},
        {"HALL, still lows off (one tank)", 1, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, false, false, false},
        {"MASSIVE, defaults (still lows on)", 2, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, false, false},
        {"MASSIVE, still lows off (one tank)", 2, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, false, false, false},
        {"PLATE, frozen + twin playing", 0, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, true, false},
        {"HALL, frozen + twin playing", 1, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, true, false},
        {"MASSIVE, frozen + twin playing", 2, 0.5f, 0.2f, 0.5f, 0.2f, 0.0f, true, true, false},
        {"MASSIVE, ERA full + DRIFT full Wander", 2, 0.5f, 1.0f, 1.0f, 0.2f, 0.0f, true, false, false},
        {"MASSIVE, BLOOM, TIME full", 2, 0.5f, 0.2f, 0.5f, 1.0f, 0.5f, true, false, false},
        {"MASSIVE, REVERSE, TIME full", 2, 0.5f, 0.2f, 0.5f, 1.0f, 1.0f, true, false, false},
        {"MASSIVE worst: frozen + twin, ERA full, DRIFT CW, BLOOM", 2, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, true, true, false},
        {"HALL, 5 min of silence after a tail", 1, 0.6f, 0.2f, 0.5f, 0.2f, 0.0f, true, false, true},
        {"MASSIVE, 5 min of silence after a tail", 2, 0.75f, 0.2f, 0.5f, 0.2f, 0.0f, true, false, true},
    };
    std::vector<float> inL(block), inR(block), outL(block), outR(block);
    double ref = 0.0;
    for (const CpuCase& c : cases) {
        std::vector<double> reps;
        double p99Worst = 0.0;
        for (int rep = 0; rep < 3; ++rep) {
            iv::Engine e;
            e.prepare(fs, block);
            e.setStillLows(c.stillLows);
            e.setParam(ParamId::Space, iv::switchToNormalised(c.space));
            e.setParam(ParamId::Decay, c.decay);
            e.setParam(ParamId::Era, c.era);
            e.setParam(ParamId::Drift, c.drift);
            e.setParam(ParamId::Time, c.time);
            e.setParam(ParamId::Attack, c.attack);
            e.setParam(ParamId::Mix, 1.0f);
            e.reset();
            Rng r(99);
            uint32_t ns = 1;
            int64_t s = 0;
            auto fill = [&](float level) {
                for (int i = 0; i < block; ++i, ++s) {
                    ns = ns * 1664525u + 1013904223u;
                    const float w = float(int32_t(ns)) * (1.0f / 2147483648.0f);
                    const float tone = 0.2f * std::sin(float(s) * 2.0f * 3.14159265f * 220.0f / fs);
                    inL[size_t(i)] = level * (0.3f * w + tone);
                    inR[size_t(i)] = level * (0.3f * w - tone);
                }
            };
            const int warm = int(5.0f * fs / block);
            for (int b = 0; b < warm; ++b) { fill(1.0f); if (c.freeze && b == warm / 2) e.button(true, 0); if (c.freeze && b == warm / 2 + 2) e.button(false, 0); e.process(inL.data(), inR.data(), outL.data(), outR.data(), block); }
            if (c.silentTail) {
                for (int b = 0; b < int(300.0f * fs / block); ++b) { fill(0.0f); e.process(inL.data(), inR.data(), outL.data(), outR.data(), block); }
            }
            const int nb = int(20.0f * fs / block);
            std::vector<double> times;
            times.reserve(size_t(nb));
            for (int b = 0; b < nb; ++b) {
                fill(c.silentTail ? 0.0f : 1.0f);
                const auto t0 = std::chrono::steady_clock::now();
                e.process(inL.data(), inR.data(), outL.data(), outR.data(), block);
                times.push_back(double(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count()));
            }
            double sum = 0.0;
            for (double v : times) sum += v;
            reps.push_back(sum / double(times.size()));
            std::sort(times.begin(), times.end());
            p99Worst = std::max(p99Worst, times[size_t(double(times.size()) * 0.99)]);
        }
        std::sort(reps.begin(), reps.end());
        const double med = reps[1];
        if (ref == 0.0 && std::string(c.name).find("HALL, defaults") != std::string::npos) ref = med;
        std::fprintf(log, "{\"kind\":\"cpu\",\"case\":\"%s\",\"us_per_block48\":%.2f,\"p99_us\":%.2f,\"rel_hall\":%.2f}\n", c.name, med / 1000.0,
                     p99Worst / 1000.0, ref > 0.0 ? med / ref : 0.0);
        std::fflush(log);
    }
}

} // namespace

int main(int argc, char** argv)
{
    Soak s;
    bool cpu = false;
    std::string logPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--seed") s.seed = std::strtoull(next().c_str(), nullptr, 10);
        else if (a == "--rate") s.fs = std::stof(next());
        else if (a == "--block") s.blockMode = std::stoi(next());
        else if (a == "--minutes") s.minutes = std::stod(next());
        else if (a == "--log") logPath = next();
        else if (a == "--dump-at") s.dumpAt = std::stoll(next());
        else if (a == "--dump-seconds") s.dumpSeconds = std::stod(next());
        else if (a == "--dump") s.dumpPath = next();
        else if (a == "--no-still-lows") s.stillLows = false;
        else if (a == "--trace-episode") s.traceEpisode = std::stoi(next());
        else if (a == "--cpu") cpu = true;
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }
    FILE* log = stdout;
    if (!logPath.empty()) {
        log = std::fopen(logPath.c_str(), "w");
        if (!log) { std::fprintf(stderr, "cannot open %s\n", logPath.c_str()); return 2; }
    }
    if (cpu) { cpuTable(log); return 0; }
    s.log = log;
    if (s.dumpAt >= 0 && s.dumpPath.empty()) s.dumpPath = "soak_dump.wav";
    s.soak();
    return 0;
}
