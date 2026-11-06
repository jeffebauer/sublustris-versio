#pragma once
// The knob position that puts DECAY's scale at `scale` (the inverse of iv::map::decayScale, v1.0.72, ADR 0048). Tests
// pin a tail by its place on DECAY's scale (0-0.80 finite, 0.85 endless on), which every voicing number is keyed to, so
// they keep measuring the same tail wherever the knob puts it. A test of the knob itself uses the knob.
#include "params/Mappings.h"

#include <cmath>

inline float decayKnobFor(float scale)
{
    using namespace iv::map;
    if (scale <= kDecayFiniteTop) {
        const float y = scale / kDecayFiniteTop, b = kDecayKnobBend;
        return kDecayKnobFiniteTop * (((1.0f + b) - std::sqrt((1.0f + b) * (1.0f + b) - 4.0f * b * y)) / (2.0f * b));
    }
    if (scale < kDecayInfiniteAt)
        return kDecayKnobFiniteTop + (scale - kDecayFiniteTop) / 0.05f * (kDecayKnobEndlessAt - kDecayKnobFiniteTop);
    return kDecayKnobEndlessAt + (scale - kDecayInfiniteAt) / 0.15f * (1.0f - kDecayKnobEndlessAt);
}
