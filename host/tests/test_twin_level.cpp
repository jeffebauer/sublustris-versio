// The freeze twin's level against the frozen pad. v1.0.41 (the owner's pick on renders/twin_louder: "Let's shoot
// for 8db under the pad") set HALL's and MASSIVE's twin tank there; v1.0.43 (the owner's pick E on
// renders/small_room) replaced it with a small room at the page's gains (voicing::kTwinRoomHallDb /
// kTwinRoomMassiveDb), matched to v1.0.41's twin at the page's setting, SIZE 0.9 and DECAY 0.9. PLATE's twin
// (a second Plate) is as before.
//
// Two runs per setting: the pad (music 0-3 s, a tap at 2.5 s, then silence) and the twin (silence frozen at
// 2.5 s, the same music played over it from 3 s). Their K-weighted levels over 5-9 s (BS.1770's filters, as
// test_era's loudness), twin minus pad, on the dub skank (test_audio/stimulus/04_skank.wav).
//
// Asserted: at the page's setting, HALL's and MASSIVE's room within 1.5 dB of where v1.0.41's twin sat (its figure
// measured by this test against v1.0.41); PLATE's twin within 0.2 dB of v1.0.41's. Across SIZE 0 / 0.5 / 0.9 x
// DECAY 0.5 / 0.9 the room follows the pad (v1.0.43, the owner's pick B on renders/room_level), so it keeps
// v1.0.41's distance under the pad within 3 dB (the page's constant gain sat up to 8 dB closer); HALL at SIZE 0
// within 4 dB since v1.0.47 (the owner chose the scaled wander at the room_motion page's level, 0.93 dB under the
// unscaled room there). Every room figure is pinned within 0.3 dB of v1.0.47's.

#include "Wav.h"
#include "dsp/Engine.h"

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

constexpr float kSr = 48000.0f;
constexpr int   kBlock = 48;

// BS.1770's K-weighting at 48 kHz (the head's shelf, then the RLB high-pass), dB of the mean power.
double kWeightedDb(const std::vector<float>& l, const std::vector<float>& r)
{
    double sum = 0.0;
    for (const auto* ch : {&l, &r}) {
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
    return 10.0 * std::log10(sum / double(2 * l.size()) + 1e-30) - 0.691;
}

// K-weighted level (dB) over 5-9 s: the pad (padRun) or the twin.
double level(const std::vector<float>& music, int space, float size, float decay, bool padRun)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->setParam(iv::ParamId::Size, size);
    e->setParam(iv::ParamId::Decay, decay);
    e->setParam(iv::ParamId::Era, 0.0f); // the Neve alone (as the pages)
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(9.0f * kSr) / kBlock, press = int(2.5f * kSr) / kBlock, tap = int(0.1f * kSr) / kBlock;
    std::vector<float> l(kBlock), ol(kBlock), orr(kBlock), keepL, keepR;
    for (int b = 0; b < blocks; ++b) {
        const size_t at = size_t(b * kBlock);
        const float t = float(at) / kSr;
        for (int i = 0; i < kBlock; ++i) {
            const size_t k = padRun ? at + size_t(i) : at + size_t(i) - size_t(3.0f * kSr); // the twin run: from 3 s
            const bool on = padRun ? t < 3.0f : t >= 3.0f;
            l[size_t(i)] = on && k < music.size() ? music[k] : 0.0f;
        }
        if (b == press) e->button(true, 0);
        if (b == press + tap) e->button(false, 0);
        e->process(l.data(), l.data(), ol.data(), orr.data(), kBlock);
        if (t >= 5.0f) {
            keepL.insert(keepL.end(), ol.begin(), ol.end());
            keepR.insert(keepR.end(), orr.begin(), orr.end());
        }
    }
    return kWeightedDb(keepL, keepR);
}

} // namespace

int main()
{
    iv::wav::Audio a;
    std::string error;
    bool read = false;
    for (const char* prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (!read) read = iv::wav::read(std::string(prefix) + "04_skank.wav", a, error);
    if (!read) {
        check(false, "twin level: test_audio/stimulus/04_skank.wav (python3 tools/make_stimulus.py)");
        return 1;
    }
    const std::vector<float>& music = a.channels[0];
    struct Case {
        const char* name;
        int space;
        float size, decay;
        double v1041; // twin - pad, dB, measured by this test against v1.0.41 (the twin tank)
        double limit; // asserted within this of it (0: printed only)
        double v1047; // twin - pad, dB, as the owner heard it on renders/room_motion (v1.0.47: the wander scaled with the
                      // lines, at the page's level trims, voicing::kTwinRoomTrimDb), asserted within 0.3 dB
    };
    // The page's setting first (the anchor), then the grid of renders/room_level.
    const Case cases[] = {
        {"MASSIVE S0.9 D0.9 (the page's)", 2, 0.9f, 0.9f, -4.04, 1.5, -4.18},
        {"HALL S0.9 D0.9 (the page's)", 1, 0.9f, 0.9f, -8.29, 1.5, -9.63},
        {"PLATE S0.9 D0.9", 0, 0.9f, 0.9f, -10.00, 0.2, -10.00},
        {"MASSIVE S0 D0.5", 2, 0.0f, 0.5f, -4.76, 3.0, -5.11},
        {"MASSIVE S0 D0.9", 2, 0.0f, 0.9f, -4.84, 3.0, -5.59},
        {"MASSIVE S0.5 D0.5", 2, 0.5f, 0.5f, -1.51, 3.0, -0.69},
        {"MASSIVE S0.5 D0.9", 2, 0.5f, 0.9f, -4.45, 3.0, -3.60},
        {"MASSIVE S0.9 D0.5", 2, 0.9f, 0.5f, -3.18, 3.0, -2.95},
        {"HALL S0 D0.5", 1, 0.0f, 0.5f, -5.51, 4.0, -8.71},
        {"HALL S0 D0.9", 1, 0.0f, 0.9f, -5.11, 4.0, -9.00},
        {"HALL S0.5 D0.5", 1, 0.5f, 0.5f, -9.13, 3.0, -10.27},
        {"HALL S0.5 D0.9", 1, 0.5f, 0.9f, -7.68, 3.0, -9.18},
        {"HALL S0.9 D0.5", 1, 0.9f, 0.5f, -10.13, 3.0, -11.32},
    };
    for (const auto& c : cases) {
        const double d = level(music, c.space, c.size, c.decay, false) - level(music, c.space, c.size, c.decay, true);
        std::printf("%-32s twin - pad %+6.2f dB (v1.0.41's twin %+.2f, %+.1f)\n", c.name, d, c.v1041, d - c.v1041);
        if (c.limit > 0.0) check(std::fabs(d - c.v1041) <= c.limit, std::string(c.name) + ": within " + std::to_string(c.limit).substr(0, 3) + " dB of v1.0.41's twin");
        check(std::fabs(d - c.v1047) <= 0.3, std::string(c.name) + ": within 0.3 dB of v1.0.47's (the room_motion page's level)");
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
