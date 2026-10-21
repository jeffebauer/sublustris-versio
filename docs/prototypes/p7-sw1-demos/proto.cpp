// Throwaway listening sketch (owner request, 21 Oct 2026): not the spec'd DSP,
// and none of it goes into core/. An 8-line FDN reverb with the P7 and SW1
// candidates switchable from the command line, so the owner can hear what
// each control *does* before choosing. Tone is rough on purpose.
//
// Build: clang++ -O2 -std=c++17 proto.cpp -o proto
// Run:   proto in.f32 out.f32 key=value ...   (raw float32 stereo, 48 kHz)
//
// Keys (defaults in P):
//   size      mean line delay, ms          decay   T60 s (0 = infinite)
//   damp      loop low-pass, Hz            mix     0..1 equal-power
//   desk      0 off, else 0.05..1 knob     desksweep 1: desk rides 0.05 -> 1 -> 0.05
//   depth     drift depth, cents           rate    drift rate, Hz
//   duck      0 none, 1 darken, 2 push back, 3 level
//   lowcut    send high-pass, Hz (0 off)   bloom   1: slow swell before the tank
//   tail      seconds of silence appended
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr float kFs = 48000.0f;
constexpr float kPi = 3.14159265358979f;
constexpr int kN = 8;

std::map<std::string, float> P = {
    {"size", 140}, {"decay", 6}, {"damp", 7000}, {"mix", 0.5f}, {"desk", 0}, {"desksweep", 0},
    {"depth", 3}, {"rate", 0.3f}, {"duck", 0}, {"lowcut", 0}, {"bloom", 0}, {"tail", 8},
};

struct Rng {
    uint32_t s;
    float next() { s = s * 1664525u + 1013904223u; return (s >> 8) * (1.0f / 16777216.0f) * 2.0f - 1.0f; }
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void peaking(float f, float q, float db)
    {
        const float A = std::pow(10.0f, db / 40.0f), w = 2 * kPi * f / kFs, al = std::sin(w) / (2 * q);
        const float a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * std::cos(w) / a0; b2 = (1 - al * A) / a0;
        a1 = b1; a2 = (1 - al / A) / a0;
    }
    void highpass(float f, float q)
    {
        const float w = 2 * kPi * f / kFs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
};

struct Allpass {
    std::vector<float> buf;
    int pos = 0;
    explicit Allpass(int n = 1) : buf(n, 0.0f) {}
    float process(float x, float g)
    {
        const float d = buf[pos];
        const float v = x + g * d;
        buf[pos] = v;
        pos = (pos + 1) % int(buf.size());
        return d - g * v;
    }
};

// Cubic soft clip: slope 1 at 0, flat at +-k.
float softClip(float x, float k = 1.2f)
{
    const float u = std::fmax(-1.5f, std::fmin(1.5f, x / k));
    return k * (u - u * u * u * (4.0f / 27.0f));
}

bool isPrime(int n)
{
    for (int d = 2; d * d <= n; ++d)
        if (n % d == 0) return false;
    return n > 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: proto in.f32 out.f32 key=value...\n"); return 1; }
    for (int i = 3; i < argc; ++i) {
        const char* eq = std::strchr(argv[i], '=');
        if (!eq || !P.count(std::string(argv[i], size_t(eq - argv[i])))) { std::fprintf(stderr, "bad arg %s\n", argv[i]); return 1; }
        P[std::string(argv[i], size_t(eq - argv[i]))] = std::strtof(eq + 1, nullptr);
    }

    std::vector<float> in;
    {
        FILE* f = std::fopen(argv[1], "rb");
        if (!f) { std::perror(argv[1]); return 1; }
        float b[4096];
        size_t n;
        while ((n = std::fread(b, sizeof(float), 4096, f)) > 0) in.insert(in.end(), b, b + n);
        std::fclose(f);
    }
    const size_t inFrames = in.size() / 2;
    const size_t frames = inFrames + size_t(P["tail"] * kFs);
    in.resize(frames * 2, 0.0f);

    // Lines: mutually prime lengths spread around the mean.
    const float ratios[kN] = {0.62f, 0.71f, 0.83f, 0.94f, 1.05f, 1.17f, 1.31f, 1.43f};
    int len[kN];
    for (int i = 0; i < kN; ++i) {
        int n = int(ratios[i] * P["size"] * 0.001f * kFs);
        while (!isPrime(n)) ++n;
        len[i] = n;
    }
    constexpr int kModRoom = 4096;
    std::vector<float> line[kN];
    int wpos[kN] = {};
    for (int i = 0; i < kN; ++i) line[i].assign(len[i] + kModRoom + 4, 0.0f);

    const bool infinite = P["decay"] <= 0.0f;
    float gain[kN];
    for (int i = 0; i < kN; ++i) gain[i] = infinite ? 1.0f : std::pow(10.0f, -3.0f * len[i] / (P["decay"] * kFs));

    // Modulation: a sine per line (DRIFT right side) plus a slow random floor.
    float lfoPhase[kN], lfoRate[kN], floorVal[kN] = {}, floorTarget[kN] = {};
    Rng rng{12345};
    for (int i = 0; i < kN; ++i) {
        lfoPhase[i] = (i + 0.5f) / kN;
        lfoRate[i] = P["rate"] * (0.82f + 0.36f * i / (kN - 1));
    }
    auto centsToSamples = [](float cents, float hz) {
        const float dev = std::pow(2.0f, cents / 1200.0f) - 1.0f;
        return std::fmin(float(kModRoom - 64) * 0.5f, dev / (2 * kPi * hz / kFs));
    };

    // Loop filters.
    float lp[kN] = {}, hpX[kN] = {}, hpY[kN] = {};
    Biquad desk[kN];
    const float hpC = std::exp(-2 * kPi * 12.0f / kFs);

    // Input diffusion (Dattorro's lengths, scaled to 48 kHz), per channel.
    const int apLen[4] = {229, 173, 613, 449};
    std::vector<Allpass> diff[2];
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) diff[c].emplace_back(apLen[k] + 37 * c);
    // Bloom: long allpasses smear the onset into a swell.
    const int blLen[4] = {7213, 11027, 15511, 21997};
    std::vector<Allpass> bloom[2];
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) bloom[c].emplace_back(blLen[k] + 101 * c);
    Biquad lowcut[2];
    for (auto& b : lowcut) b.highpass(std::fmax(P["lowcut"], 10.0f), 0.707f);

    float env = 0.0f;
    const float envAtt = 1 - std::exp(-1 / (0.005f * kFs)), envRel = 1 - std::exp(-1 / (0.4f * kFs));
    const int duckStyle = int(P["duck"]);
    const float th = std::cos(P["mix"] * kPi / 2), tw = std::sin(P["mix"] * kPi / 2);

    std::vector<float> out(frames * 2);
    float deskX = P["desk"], deskMake = 1, deskDrive = 1, lpC = 0;
    for (size_t n = 0; n < frames; ++n) {
        // Control rate: every 32 samples.
        if (n % 32 == 0) {
            if (P["desksweep"] > 0) {
                const float t = float(n) / float(inFrames);
                deskX = t < 0.5f ? 0.05f + 1.9f * t : (t < 1 ? 0.05f + 1.9f * (1 - t) : 0.05f);
            }
            float peak = 1;
            if (deskX >= 0.05f) {
                const float x = (deskX - 0.05f) / 0.95f;
                const float f = 150 * std::pow(4000.0f / 150.0f, x), q = 0.7f + 7.3f * x, db = 9 * x;
                for (auto& d : desk) d.peaking(f, q, db);
                peak = std::pow(10.0f, db / 20.0f);
                deskDrive = std::pow(10.0f, 12 * x / 20.0f);
            } else {
                for (auto& d : desk) d = Biquad{};
                deskDrive = 1;
            }
            deskMake = 1 / peak; // loop gain at the peak stays <= 1: the peak rings longest
            const float d = duckStyle == 1 ? std::fmin(1.0f, env) : 0.0f;
            const float cut = P["damp"] * std::pow(1500.0f / P["damp"], d);
            lpC = 1 - std::exp(-2 * kPi * cut / kFs);
            for (int i = 0; i < kN; ++i)
                if ((n / 32) % 300 == size_t(i) * 37 % 300) floorTarget[i] = rng.next();
        }

        float xl = in[2 * n], xr = in[2 * n + 1];
        // Duck amount from the input level: -42 dBFS -> 0, -24 dBFS -> 1.
        const float a = std::fmax(std::fabs(xl), std::fabs(xr));
        env += (a > env ? envAtt : envRel) * (a - env);
        const float envDb = 20 * std::log10(env + 1e-9f);
        const float duck = duckStyle ? std::fmin(1.0f, std::fmax(0.0f, (envDb + 42) / 18)) : 0.0f;

        if (P["lowcut"] > 0) { xl = lowcut[0].process(xl); xr = lowcut[1].process(xr); }
        const float g = duckStyle == 2 ? 0.7f + 0.2f * duck : 0.7f;
        for (auto& ap : diff[0]) xl = ap.process(xl, g);
        for (auto& ap : diff[1]) xr = ap.process(xr, g);
        if (P["bloom"] > 0) {
            for (auto& ap : bloom[0]) xl = ap.process(xl, 0.65f);
            for (auto& ap : bloom[1]) xr = ap.process(xr, 0.65f);
        }

        // Read the lines.
        float y[kN];
        for (int i = 0; i < kN; ++i) {
            lfoPhase[i] += lfoRate[i] / kFs;
            if (lfoPhase[i] >= 1) lfoPhase[i] -= 1;
            floorVal[i] += 0.00002f * (floorTarget[i] - floorVal[i]);
            const float mod = centsToSamples(P["depth"], std::fmax(P["rate"], 0.02f)) * std::sin(2 * kPi * lfoPhase[i])
                + centsToSamples(1.5f, 0.3f) * floorVal[i];
            const float dl = float(len[i]) + kModRoom * 0.5f + mod;
            const int sz = int(line[i].size());
            float rp = float(wpos[i]) - dl;
            while (rp < 0) rp += float(sz);
            const int i1 = int(rp);
            const float t = rp - float(i1);
            const float x0 = line[i][(i1 - 1 + sz) % sz], x1 = line[i][i1], x2 = line[i][(i1 + 1) % sz],
                        x3 = line[i][(i1 + 2) % sz];
            const float c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2 * x2 - 0.5f * x3,
                        c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            y[i] = ((c3 * t + c2) * t + c1) * t + x1;
        }

        // Outputs from the lines, before the feedback matrix.
        float ol = 0, orr = 0;
        for (int i = 0; i < kN; ++i) {
            ol += (i & 1 ? -1.0f : 1.0f) * y[i];
            orr += (i & 2 ? -1.0f : 1.0f) * y[i];
        }
        ol *= 0.35f; orr *= 0.35f;

        // Loop chain per line, then the Hadamard matrix.
        float v[kN];
        for (int i = 0; i < kN; ++i) {
            float s = y[i];
            lp[i] += lpC * (s - lp[i]);
            s = lp[i];
            if (deskX >= 0.05f) s = desk[i].process(s) * deskMake;
            s = softClip(s * deskDrive) / deskDrive;
            const float h = hpC * (hpY[i] + s - hpX[i]); // DC block in the loop
            hpX[i] = s; hpY[i] = h;
            v[i] = h * gain[i];
        }
        for (int span = 1; span < kN; span <<= 1)
            for (int i = 0; i < kN; i += 2 * span)
                for (int j = i; j < i + span; ++j) {
                    const float p = v[j], q = v[j + span];
                    v[j] = p + q; v[j + span] = p - q;
                }
        const float norm = 1.0f / std::sqrt(float(kN));
        for (int i = 0; i < kN; ++i) {
            const float inj = (i & 1) ? xr : xl;
            line[i][wpos[i]] = v[i] * norm + inj * ((i & 2) ? -0.5f : 0.5f);
            wpos[i] = (wpos[i] + 1) % int(line[i].size());
        }

        // Duck styles on the wet.
        float wl = ol, wr = orr;
        if (duckStyle == 2) { // push back: narrower
            const float m = 0.5f * (wl + wr), s = 0.5f * (wl - wr) * (1 - 0.6f * duck);
            wl = m + s; wr = m - s;
        }
        float wetGain = 1 - 0.29f * duck; // every style dips ~3 dB at full
        if (duckStyle == 3) wetGain = std::pow(10.0f, -12 * duck / 20);
        wl *= wetGain; wr *= wetGain;
        out[2 * n] = th * in[2 * n] + tw * wl;
        out[2 * n + 1] = th * in[2 * n + 1] + tw * wr;
    }

    // Keep the file under -1 dBFS (a sketch has no limiter); report the peak.
    float peak = 0;
    for (float s : out) peak = std::fmax(peak, std::fabs(s));
    const float trim = peak > 0.89f ? 0.89f / peak : 1.0f;
    for (float& s : out) s *= trim;
    std::fprintf(stderr, "peak %.2f dBFS, trim %.2f dB\n", 20 * std::log10(peak + 1e-12f), 20 * std::log10(trim));

    FILE* f = std::fopen(argv[2], "wb");
    if (!f) { std::perror(argv[2]); return 1; }
    std::fwrite(out.data(), sizeof(float), out.size(), f);
    std::fclose(f);
    return 0;
}
