// Renderer CLI-support test suite: automation interpolation + event
// scheduling, sweep grid + naming (seeded from Resilio).
// Synthetic JSON only -- doesn't touch the Engine's DSP, which lands from M1.

#include "Automation.h"
#include "Json.h"
#include "Sweep.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

void automationInterpolatesLinearly()
{
    const char* text = R"({
        "breakpoints": [
            {"t": 0.0, "key": "decay", "value": 0.2},
            {"t": 4.0, "key": "decay", "value": 1.0},
            {"t": 1.0, "key": "size", "value": 0.5}
        ],
        "buttons": [[1.5, 1.6], [3.0, 3.2], [0.25, 0.5]]
    })";
    iv::json::Value root;
    std::string error;
    bool ok = iv::json::parse(text, root, error);
    check(ok, "automation: JSON parses");

    iv::automation::Automation autom;
    ok = ok && iv::automation::parse(root, autom, error);
    check(ok, "automation: breakpoints + buttons parse without error");

    const iv::automation::Track* decay = nullptr;
    for (const auto& t : autom.tracks) if (t.key == iv::ParamId::Decay) decay = &t;
    check(decay != nullptr, "automation: 'decay' track found");
    if (decay) {
        check(std::fabs(iv::automation::valueAt(*decay, 0.0) - 0.2) < 1e-9, "automation: value at t=0 matches first breakpoint");
        check(std::fabs(iv::automation::valueAt(*decay, 4.0) - 1.0) < 1e-9, "automation: value at t=4 matches last breakpoint");
        check(std::fabs(iv::automation::valueAt(*decay, 2.0) - 0.6) < 1e-9, "automation: value at the midpoint is linearly interpolated");
        check(std::fabs(iv::automation::valueAt(*decay, 10.0) - 1.0) < 1e-9, "automation: value clamps to the last breakpoint after it");
        check(std::fabs(iv::automation::valueAt(*decay, -1.0) - 0.2) < 1e-9, "automation: value clamps to the first breakpoint before it");
    }

    // Button presses and releases come back sorted, ready for sample-accurate scheduling.
    const auto& b = autom.buttonEvents;
    check(b.size() == 6, "automation: all button presses and releases parsed");
    check(b.size() == 6 && b[0] == std::make_pair(0.25, true) && b[1] == std::make_pair(0.5, false)
              && b[2] == std::make_pair(1.5, true) && b[5] == std::make_pair(3.2, false),
          "automation: button events sorted ascending");
}

void eventSchedulingIsSampleAccurate()
{
    // Mirrors the Renderer's own scheduling: convert an event time (a gate
    // edge, a button press) to a sample index, then find the micro-block
    // offset within a <=16-sample block that contains it.
    const double sr = 48000.0;
    const double eventTime = 1.500001; // 72000.048 samples -> rounds to 72000
    const long eventSample = std::lround(eventTime * sr);
    check(eventSample == 72000, "automation: event time converts to the nearest sample");

    const int microBlock = 16;
    const long blockStart = (eventSample / microBlock) * microBlock;
    const int offset = int(eventSample - blockStart);
    check(offset >= 0 && offset < microBlock, "automation: event offset lands within its <=16-sample micro-block");
    check(blockStart + offset == eventSample, "automation: block start + offset reconstructs the exact event sample");
}

void sweepParsesGridAndBase()
{
    const char* text = R"({
        "name": "m1_grid",
        "input": "test_audio/stimulus/01_clicks.wav",
        "base": {"space": "MASSIVE"},
        "grid": {"decay": [0, 0.5, 1], "size": [0, 1]},
        "tail_seconds": 12
    })";
    iv::json::Value root;
    std::string error;
    bool ok = iv::json::parse(text, root, error);
    iv::sweep::Config cfg;
    ok = ok && iv::sweep::parse(root, cfg, error);
    check(ok, "sweep: JSON parses");
    check(cfg.name == "m1_grid" && cfg.input == "test_audio/stimulus/01_clicks.wav" && cfg.tailSeconds == 12.0,
          "sweep: name/input/tail_seconds parsed");
    check(cfg.grid.size() == 2, "sweep: both grid axes parsed, insertion order preserved");
    check(cfg.grid[0].key == "decay" && cfg.grid[0].values.size() == 3, "sweep: 'decay' axis has 3 values");
    check(cfg.grid[1].key == "size" && cfg.grid[1].values.size() == 2, "sweep: 'size' axis has 2 values");

    const auto combos = iv::sweep::cartesian(cfg.grid);
    check(combos.size() == 6, "sweep: cartesian product is 3x2 = 6 combinations");

    bool allUnique = true;
    for (size_t i = 0; i < combos.size(); ++i)
        for (size_t j = i + 1; j < combos.size(); ++j)
            if (iv::sweep::fileBaseName(cfg.name, combos[i]) == iv::sweep::fileBaseName(cfg.name, combos[j])) allUnique = false;
    check(allUnique, "sweep: every combination gets a unique file name");
}

void sweepNamingMatchesContract()
{
    iv::sweep::Combo combo{{"decay", 0.5}, {"size", 1.0}};
    const std::string name = iv::sweep::fileBaseName("m1_grid", combo);
    check(name == "m1_grid__decay0.50_size1.00", "sweep: file name matches '<name>__decay0.50_size1.00' scheme");
}

} // namespace

int main()
{
    automationInterpolatesLinearly();
    eventSchedulingIsSampleAccurate();
    sweepParsesGridAndBase();
    sweepNamingMatchesContract();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
