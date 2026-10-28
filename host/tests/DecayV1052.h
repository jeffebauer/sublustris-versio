#pragma once
// The DECAY that rings as long a tail as `decay` did before v1.0.53-decay (ADR 0034: PLATE's and HALL's finite tops
// 12 / 25 s -> 30 / 60 s on the same exponential curve). Tests that pin a page's measure at a tail length (the room's
// level against the pad, MOTION's pitch spread) use it, so they keep measuring the same tail. MASSIVE and the
// crossover / infinite (DECAY >= 0.80) are unchanged.
#include "params/Mappings.h"
#include "params/SpaceVoicing.h"

#include <cmath>

inline float decaySameTailAsV1052(float decay, int space)
{
    if (space == 2 || decay >= iv::map::kDecayFiniteTop) return decay;
    const iv::map::T60Range& r = iv::map::t60Range(space);
    return decay * std::log(iv::voicing::kDecayLevelTopSeconds[space] / r.minS) / std::log(r.maxS / r.minS);
}
