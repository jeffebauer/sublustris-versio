#pragma once
// Automation JSON: parameter breakpoints over time + gate and button events.
// Linear interpolation between breakpoints per key.
//
// { "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.2}, ... ],
//   "gates":   [[1.0, 1.25], [3.0, 3.5]],   // rise, fall (seconds)
//   "buttons": [[5.0, 5.1], [5.3, 7.5]] }   // press, release (seconds)
//
// Both are sample-accurate (Engine::gate, Engine::button). The gate mirrors
// the button (ADR 0011): a rise is a press, a fall a release.

#include "Json.h"
#include "dsp/Engine.h"

#include <string>
#include <utility>
#include <vector>

namespace iv::automation {

struct Track {
    ParamId key;
    std::vector<std::pair<double, double>> points; // (t seconds, value), sorted by t
};

struct Automation {
    std::vector<Track> tracks;         // one per automated key, insertion order
    // Gate changes (seconds, high?), sorted by time; from "gates" [rise, fall] pairs.
    std::vector<std::pair<double, bool>> gateEvents;
    // Button changes (seconds, down?), sorted by time; from "buttons" [press, release] pairs.
    std::vector<std::pair<double, bool>> buttonEvents;
};

// Linear interpolation; clamps to the first/last point outside the range.
double valueAt(const Track& track, double t);

bool parse(const json::Value& root, Automation& out, std::string& error);
bool loadFile(const std::string& path, Automation& out, std::string& error);

} // namespace iv::automation
