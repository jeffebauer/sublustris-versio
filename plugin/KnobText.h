#pragma once
// Knob value text, one place for the plugin window and the host's own parameter view (Ableton's
// device panel, automation lanes): percent of travel; DRIFT as its side and amount (ADR 0012: left
// tape wow/flutter, right wander, noon still); WEAR (ParamId::Era, key "era") as its amount (v1.0.42, one wear amount over
// the Neve base: "Wear 40 %"; until then the era whose fifth it was in).
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
        if (v > voicing::kDriftDeadHi) return "Radio " + pct((v - voicing::kDriftDeadHi) / (1.0f - voicing::kDriftDeadHi));
        return "Still";
    }
    if (id == ParamId::Era) return "Wear " + pct(std::clamp(v, 0.0f, 1.0f));
    return pct(v);
}

} // namespace iv::plugin
