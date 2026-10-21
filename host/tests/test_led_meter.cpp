// Release firmware LED meters (ADR 0031, SPEC §3 LEDs): firmware/LedMeter.h
// is pure maths, tested here on desktop. Level -> colour, ballistics (fast
// rise, ~0.3 s fall), red hold, and the red triggers: input near clip and
// the Engine's output limiter (Engine::limiterGain(), read-only) pulling down.
// Also the PWM tables the release firmware streams to the LED pins by DMA
// (Resilio's fix for "LEDs flicker rather than dim"). Seeded from Resilio
// (its ADR 0031); the limiter-from-Engine check returns once the limiter
// lands (SPEC §4.10).

#include "../../firmware/LedMeter.h"

#include "params/ParamSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int  failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using ivled::LevelMeter;
using ivled::Rgb;

bool isOff(const Rgb& c) { return c.r == 0.0f && c.g == 0.0f && c.b == 0.0f; }
float brightest(const Rgb& c) { return std::max({c.r, c.g, c.b}); }
float dbfs(float db) { return ivled::dbToGain(db); }

void levelScale()
{
    check(ivled::levelToPosition(0.0f) == 0.0f, "silence -> position 0");
    check(ivled::levelToPosition(dbfs(-60.0f)) == 0.0f, "-60 dBFS (below the -48 floor) -> position 0");
    check(std::fabs(ivled::levelToPosition(dbfs(-24.0f)) - 0.5f) < 1e-4f, "-24 dBFS -> position 0.5 (dB scale)");
    check(std::fabs(ivled::levelToPosition(1.0f) - 1.0f) < 1e-6f, "0 dBFS -> position 1");
    check(ivled::levelToPosition(2.0f) == 1.0f, "over full scale clamps to 1");
}

void colours()
{
    check(isOff(ivled::positionColour(0.0f)), "floor: LED off");

    const Rgb quiet = ivled::positionColour(ivled::levelToPosition(dbfs(-40.0f)));
    std::snprintf(msg, sizeof msg, "-40 dBFS glows green (r %.3f g %.3f b %.3f; drive >= kMinGlow so it shows after the PWM's cube)",
                  double(quiet.r), double(quiet.g), double(quiet.b));
    check(quiet.r == 0.0f && quiet.b == 0.0f && quiet.g >= ivled::kMinGlow, msg);

    const Rgb mid = ivled::positionColour(ivled::levelToPosition(dbfs(-20.0f)));
    check(mid.r == 0.0f && mid.g > quiet.g, "-20 dBFS: still green, brighter");

    const Rgb warm = ivled::positionColour(ivled::levelToPosition(dbfs(-12.0f)));
    check(warm.r > 0.0f && warm.g >= warm.r, "-12 dBFS: warming (red joins the green: yellow)");

    const Rgb hot = ivled::positionColour(ivled::levelToPosition(dbfs(-3.0f)));
    std::snprintf(msg, sizeof msg, "-3 dBFS: amber (r %.3f g %.3f b %.3f)", double(hot.r), double(hot.g), double(hot.b));
    check(hot.r > hot.g && hot.g > 0.5f * hot.r && hot.b == 0.0f, msg);

    // Brightness never drops as level rises; level alone never makes red.
    bool monotonic = true, neverRed = true;
    float last = 0.0f;
    for (int db = -60; db <= 6; ++db) {
        const Rgb c = ivled::positionColour(ivled::levelToPosition(dbfs(float(db))));
        monotonic &= brightest(c) >= last - 1e-6f;
        last = brightest(c);
        if (!isOff(c)) neverRed &= c.g >= 0.5f * c.r;
    }
    check(monotonic, "brightness rises monotonically with level, -60..+6 dBFS");
    check(neverRed, "level alone never turns an LED red (green stays >= half the red)");
}

void ballistics()
{
    constexpr float dt = 0.001f; // the firmware main loop runs about every 1 ms
    LevelMeter m;
    m.update(1.0f, false, dt);
    check(m.position() == 1.0f, "fast rise: one update at 0 dBFS reaches full");

    for (int i = 0; i < 300; ++i) m.update(0.0f, false, dt);
    std::snprintf(msg, sizeof msg, "fall: 0.3 s after the signal stops, position %.3f ~ e^-1", double(m.position()));
    check(std::fabs(m.position() - std::exp(-1.0f)) < 0.01f, msg);

    for (int i = 0; i < 1200; ++i) m.update(0.0f, false, dt);
    check(isOff(m.colour()), "dark 1.5 s after the signal stops");

    // A held level settles on that level (falls to it, not below).
    LevelMeter h;
    h.update(1.0f, false, dt);
    for (int i = 0; i < 3000; ++i) h.update(dbfs(-24.0f), false, dt);
    check(std::fabs(h.position() - 0.5f) < 1e-3f, "held -24 dBFS after a 0 dBFS peak settles at position 0.5");
}

void redHold()
{
    constexpr float dt = 0.001f;
    check(ivled::inputNearClip(dbfs(-0.5f)), "input at -0.5 dBFS is near clip");
    check(!ivled::inputNearClip(dbfs(-3.0f)), "input at -3 dBFS is not");
    check(ivled::limiterReducing(dbfs(-1.0f)), "limiter pulling 1 dB -> red");
    check(!ivled::limiterReducing(1.0f), "limiter at unity -> not red");
    check(!ivled::limiterReducing(dbfs(-0.2f)), "limiter pulling 0.2 dB -> not red (below 0.5 dB)");

    LevelMeter m;
    m.update(dbfs(-0.5f), ivled::inputNearClip(dbfs(-0.5f)), dt);
    const Rgb c = m.colour();
    check(m.isRed() && c.r == 1.0f && c.g == 0.0f && c.b == 0.0f, "trigger -> full red");
    int redMs = 0;
    while (m.isRed() && redMs < 5000) {
        m.update(0.0f, false, dt);
        ++redMs;
    }
    std::snprintf(msg, sizeof msg, "red holds %d ms after the last trigger (~500)", redMs);
    check(redMs >= 495 && redMs <= 505, msg);
    check(!m.isRed() && m.colour().r == 0.0f, "after the hold: back to the level colour");

    // A new trigger during the hold restarts it.
    LevelMeter r;
    r.update(1.0f, true, dt);
    for (int i = 0; i < 400; ++i) r.update(0.0f, false, dt);
    r.update(1.0f, true, dt);
    for (int i = 0; i < 400; ++i) r.update(0.0f, false, dt);
    check(r.isRed(), "a second trigger restarts the 0.5 s hold");
}

void pwm()
{
    constexpr int N = ivled::kPwmSteps;
    check(N >= 256, "PWM has at least 256 steps per period");
    check(ivled::pwmCount(0.0f) == 0 && ivled::pwmCount(-1.0f) == 0, "drive 0 (or below) -> 0 steps on");
    check(ivled::pwmCount(1.0f) == N && ivled::pwmCount(2.0f) == N, "drive 1 (or above) -> on all period");
    check(ivled::pwmCount(0.5f) == int(0.125f * float(N) + 0.5f), "drive is cubed (0.5 -> 1/8 duty)");

    // The quietest lit meter step: visibly on, clearly dim.
    const Rgb   quietest = ivled::positionColour(ivled::kOffPosition + 1e-4f);
    const int   q        = ivled::pwmCount(brightest(quietest));
    const float duty     = float(q) / float(N);
    std::snprintf(msg, sizeof msg, "quietest lit step: %d of %d steps (%.1f %% duty), between 1 %% and 5 %%", q, N,
                  double(100.0f * duty));
    check(duty >= 0.01f && duty <= 0.05f, msg);

    // Smooth dimming: many distinct, never-decreasing duties across the meter.
    int  distinct = 0, last = -1;
    bool rising   = true;
    for (int i = 0; i <= 4800; ++i) {
        const float db = -48.0f + 48.0f * float(i) / 4800.0f;
        const int   n  = ivled::pwmCount(brightest(ivled::positionColour(ivled::levelToPosition(dbfs(db)))));
        rising &= n >= last;
        if (n != last) ++distinct;
        last = n;
    }
    std::snprintf(msg, sizeof msg,
                  "meter -48..0 dBFS covers %d distinct brightness steps (>= 64), never dimming as level rises",
                  distinct);
    check(distinct >= 64 && rising, msg);

    // Active-low pins: on = reset bit (high half of BSRR), off = set bit.
    const ivled::PwmPin p5 = ivled::pwmPin(5, true);
    check(p5.onBits == (1u << 21) && p5.offBits == (1u << 5), "active-low pin 5: on resets it, off sets it");
    const ivled::PwmPin h5 = ivled::pwmPin(5, false);
    check(h5.onBits == (1u << 5) && h5.offBits == (1u << 21), "active-high pin 5: the other way round");

    // A port with six LED pins (like the Versio's port B), assorted duties.
    const int     pinNo[6]  = {5, 8, 9, 6, 7, 14};
    const int     counts[6] = {0, 1, 14, 256, N - 1, N};
    ivled::PwmPin pins[6];
    uint32_t      all = 0;
    for (int k = 0; k < 6; ++k) {
        pins[k] = ivled::pwmPin(pinNo[k], true);
        all |= pins[k].onBits | pins[k].offBits;
    }
    std::vector<uint32_t> words(size_t(N), 0xdeadbeefu);
    ivled::fillPwmWords(words.data(), N, pins, counts, 6);
    bool exact = true, noStray = true;
    for (int s = 0; s < N; ++s) {
        const uint32_t w = words[size_t(s)];
        noStray &= (w & ~all) == 0;
        for (int k = 0; k < 6; ++k) {
            const bool on = (w & pins[k].onBits) != 0, off = (w & pins[k].offBits) != 0;
            exact &= on != off && on == (s < counts[k]);
        }
    }
    check(exact, "PWM table: each pin on for exactly its first `count` steps, off after, every step drives it");
    check(noStray, "PWM table: no bits for pins outside the port's LED pins");

    // Rebuilding after a change leaves nothing of the old table.
    const int counts2[6] = {N, 0, 3, 3, 400, 1};
    ivled::fillPwmWords(words.data(), N, pins, counts2, 6);
    bool exact2 = true;
    for (int s = 0; s < N; ++s)
        for (int k = 0; k < 6; ++k) exact2 &= ((words[size_t(s)] & pins[k].onBits) != 0) == (s < counts2[k]);
    check(exact2, "PWM table rebuilt with new duties matches them exactly");
}

} // namespace

int main()
{
    levelScale();
    colours();
    ballistics();
    redHold();
    pwm();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
