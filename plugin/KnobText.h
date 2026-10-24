#pragma once
// Knob value text, one place for the plugin window and the host's own parameter view (Ableton's
// device panel, automation lanes): percent of travel; DRIFT as its side and amount (ADR 0012: left
// tape wow/flutter, right wander, noon still); ERA as the era whose fifth of the knob it's in and how
// far through that fifth (ADR 0021: Neve, Midiverb, BBD, Cassette, Radio; "Cassette 40 %").
#include "params/EraVoicing.h"
#include "params/ParamSpec.h"

#include <juce_core/juce_core.h>

#include <algorithm>

namespace iv::plugin {

inline juce::String knobText(ParamId id, double value)
{
    const float v = float(value);
    auto pct = [](float x) { return juce::String(juce::roundToInt(100.0f * x)) + " %"; };
    if (id == ParamId::Drift) {
        if (v < voicing::kDriftDeadLo) return "Tape " + pct((voicing::kDriftDeadLo - v) / voicing::kDriftDeadLo);
        if (v > voicing::kDriftDeadHi) return "Drift " + pct((v - voicing::kDriftDeadHi) / (1.0f - voicing::kDriftDeadHi));
        return "Still";
    }
    if (id == ParamId::Era) {
        static const char* const kNames[voicing::kEraCount] = {"Neve", "Midiverb", "BBD", "Cassette", "Radio"};
        const float pos = std::clamp(v, 0.0f, 1.0f) * float(voicing::kEraCount);
        const int   era = std::min(int(pos), voicing::kEraCount - 1); // fully CW is the radio's 100 %
        return juce::String(kNames[era]) + " " + pct(pos - float(era));
    }
    return pct(v);
}

} // namespace iv::plugin
