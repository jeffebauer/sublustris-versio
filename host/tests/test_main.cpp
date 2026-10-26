// Core + host test suite (SPEC §6.2). Dependency-free: each test returns
// true on pass. Run via ctest or directly.

#include "Wav.h"
#include "dsp/Engine.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

void paramSpecIsWellFormed()
{
    std::set<std::string> keys;
    bool ok = true;
    for (size_t i = 0; i < iv::kParams.size(); ++i) {
        const auto& p = iv::kParams[i];
        ok &= static_cast<size_t>(p.id) == i; // table order == enum order
        ok &= keys.insert(p.key).second;      // unique keys
        ok &= p.defaultValue >= 0.0f && p.defaultValue <= 1.0f;
        ok &= p.smoothingMs > 0.0f;
        if (p.kind == iv::ParamKind::Switch3)
            ok &= p.choices[0] && p.choices[1] && p.choices[2];
    }
    check(ok, "ParamSpec: ordered, unique keys, defaults in 0-1, switch labels");
}

void switchMappingRoundTrips()
{
    bool ok = true;
    for (int pos = 0; pos < 3; ++pos)
        ok &= iv::normalisedToSwitch(iv::switchToNormalised(pos)) == pos;
    check(ok, "Switch3 position <-> Normalised round-trips");
}

// MIX fully CCW is a clean passthrough, bit for bit (ADR 0012).
void enginePassthroughIsIdentity()
{
    iv::Engine engine;
    engine.prepare(48000.0f, 48);
    engine.setParam(iv::ParamId::Mix, 0.0f);
    engine.reset();
    std::vector<float> l(4800), r(4800), ol(4800), orr(4800);
    for (int i = 0; i < 4800; ++i) { l[i] = float(i % 48) / 48.0f - 0.5f; r[i] = -l[i]; }
    for (int pos = 0; pos < 4800; pos += 48)
        engine.process(l.data() + pos, r.data() + pos, ol.data() + pos, orr.data() + pos, 48);
    check(l == ol && r == orr, "Engine at MIX 0 is a bit-identical passthrough");
}

void paramsClampAndRead()
{
    iv::Engine engine;
    engine.prepare(48000.0f, 48);
    bool ok = true;
    for (const auto& p : iv::kParams) ok &= engine.param(p.id) == p.defaultValue;
    engine.setParam(iv::ParamId::Size, 1.5f);
    ok &= engine.param(iv::ParamId::Size) == 1.0f;
    engine.setParam(iv::ParamId::Time, -0.5f);
    ok &= engine.param(iv::ParamId::Time) == 0.0f;
    check(ok, "Engine starts at ParamSpec defaults and clamps to 0-1");
}

void buttonAndGateEdgesArrive()
{
    iv::Engine engine;
    engine.prepare(48000.0f, 48);
    std::vector<float> z(48, 0.0f), ol(48), orr(48);
    engine.button(true, 10);
    engine.gate(true, 3);
    engine.process(z.data(), z.data(), ol.data(), orr.data(), 48);
    bool ok = engine.buttonDown() && engine.gateHigh();
    engine.button(false, 0);
    engine.gate(false, 0);
    engine.process(z.data(), z.data(), ol.data(), orr.data(), 48);
    ok &= !engine.buttonDown() && !engine.gateHigh();
    check(ok, "button and gate edges reach the Engine (applied in the block they land in)");
}

void panelOrderMatchesAdr0017()
{
    const char* expected[] = {"BLEND", "DECAY", "TONE", "WEAR", "SIZE", "MOTION", "DEPTH", "SPACE", "ONSET"};
    bool ok = iv::kParams.size() == 9;
    for (size_t i = 0; ok && i < 9; ++i) ok &= std::string(iv::kParams[i].name) == expected[i];
    check(ok, "ParamSpec is in panel order P1-P7, SW0, SW1 (ADR 0017)");
}

void wavRoundTripIsBitIdentical()
{
    iv::wav::Audio a;
    a.sampleRate = 48000;
    a.bitsPerSample = 24;
    a.channels.assign(2, std::vector<float>(1000));
    for (int i = 0; i < 1000; ++i) {
        a.channels[0][i] = float((i * 7919) % 16777216 - 8388608) / 8388608.0f;
        a.channels[1][i] = -a.channels[0][i] * 0.5f;
    }
    // Quantise once so the reference is exactly representable in 24-bit.
    std::string err;
    const std::string p1 = "iv_test_a.wav", p2 = "iv_test_b.wav";
    iv::wav::Audio q, back;
    bool ok = iv::wav::write(p1, a, err) && iv::wav::read(p1, q, err)
           && iv::wav::write(p2, q, err) && iv::wav::read(p2, back, err);
    ok = ok && q.channels == back.channels && back.sampleRate == 48000 && back.bitsPerSample == 24;
    std::remove(p1.c_str());
    std::remove(p2.c_str());
    check(ok, "WAV 24-bit write/read round-trip is bit-identical");
}

} // namespace

int main()
{
    paramSpecIsWellFormed();
    switchMappingRoundTrips();
    enginePassthroughIsIdentity();
    paramsClampAndRead();
    buttonAndGateEdgesArrive();
    panelOrderMatchesAdr0017();
    wavRoundTripIsBitIdentical();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
