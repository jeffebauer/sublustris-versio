#pragma once
// Single source of truth for every parameter (SPEC §3, §6).
// Every Host passes Normalised values (0–1). Mapping curves to internal
// units live in params/Mappings.h and land per milestone as each DSP block
// does; until then a parameter only carries its default and smoothing tier.
// Seeded from Resilio Versio's ParamSpec (ADR 0001).
#include <array>
#include <cstddef>
#include <cstdint>

namespace iv {

// In panel order, P1–P7 then the switches (ADR 0017).
enum class ParamId : uint8_t {
    Mix,    // P1
    Decay,  // P2
    Tone,   // P3
    Era,    // P4
    Size,   // P5
    Drift,  // P6
    Time,   // P7
    Space,  // SW0
    Attack, // SW1
    Count
};

enum class ParamKind : uint8_t {
    Knob,    // continuous 0–1 (Versio knob + CV)
    Switch3  // 3-position toggle, Normalised as 0 / 0.5 / 1
};

// Smoothing tiers (as Resilio ADR 0015).
enum class Smoothing : uint8_t {
    Snappy,  // ~5 ms: level and colour, safe to move fast
    Gliding, // ~50–100 ms: moves delay lengths and filter coefficients
    Slewed,  // SIZE: slew-limited so a move bends like tape (SPEC §4.3)
    Switch   // crossfaded by the Engine (SPEC §3 "Switches")
};

struct ParamSpec {
    ParamId     id;
    // Stable identifier for presets and plugin params. Never rename one: hosts
    // save a device's parameters by it (VST3 ID = a hash of the key), so a
    // rename leaves a dead slot in every saved set. Change `name` instead.
    const char* key;
    const char* name; // panel label
    ParamKind   kind;
    float       defaultValue; // Normalised
    Smoothing   smoothing;
    float       smoothingMs;
    const char* choices[3]; // Switch3 labels, left / centre / right
};

inline constexpr std::array<ParamSpec, static_cast<size_t>(ParamId::Count)> kParams{{
    {ParamId::Mix,    "mix",    "MIX",    ParamKind::Knob,    0.5f, Smoothing::Snappy,   5.0f, {}},
    {ParamId::Decay,  "decay",  "DECAY",  ParamKind::Knob,    0.5f, Smoothing::Gliding, 80.0f, {}},
    {ParamId::Tone,   "tone",   "TONE",   ParamKind::Knob,    0.5f, Smoothing::Snappy,   5.0f, {}},
    {ParamId::Era,    "era",    "WEAR",   ParamKind::Knob,    0.0f, Smoothing::Gliding, 60.0f, {}}, // clean: the Neve alone (v1.0.42, owner: "Start Clean"); shown as WEAR, its key stays "era"
    {ParamId::Size,   "size",   "SIZE",   ParamKind::Knob,    0.5f, Smoothing::Slewed,  80.0f, {}},
    {ParamId::Drift,  "drift",  "DRIFT",  ParamKind::Knob,    0.5f, Smoothing::Gliding, 80.0f, {}},
    {ParamId::Time,   "time",   "TIME",   ParamKind::Knob,    0.2f, Smoothing::Gliding, 80.0f, {}},
    {ParamId::Space,  "space",  "SPACE",  ParamKind::Switch3, 0.5f, Smoothing::Switch, 100.0f, {"PLATE", "HALL", "MASSIVE"}},
    {ParamId::Attack, "attack", "ATTACK", ParamKind::Switch3, 0.0f, Smoothing::Switch,  50.0f, {"NORMAL", "BLOOM", "SHIMMER"}},
}};

constexpr const ParamSpec& spec(ParamId id) { return kParams[static_cast<size_t>(id)]; }

// Switch3 position (0, 1, 2) <-> Normalised value.
constexpr float switchToNormalised(int position) { return static_cast<float>(position) * 0.5f; }
constexpr int normalisedToSwitch(float v) { return v < 0.25f ? 0 : (v < 0.75f ? 1 : 2); }

} // namespace iv
