#pragma once
// Applies JSON parameter values (preset files, sweep grids, --set) to the
// Engine via the shared ParamSpec table. In JSON, knobs take a normalised
// number 0-1; Switch3 params take either a ParamSpec::choices label or a
// number 0 / 0.5 / 1. `--set` is stricter (see applySetArg).

#include "Json.h"
#include "dsp/Engine.h"

#include <string>

namespace iv::paramsjson {

bool findParamId(const std::string& key, ParamId& id);

// A Switch3 label (SPACE PLATE / HALL / CANYON, ATTACK NORMAL / BLOOM /
// SHIMMER) -> its position (0, 1, 2); the earlier labels MASSIVE and NORMAL still read.
// Returns false for anything else.
bool switchPosition(ParamId id, const std::string& label, int& position);

bool applyValue(Engine& engine, ParamId id, const json::Value& val, std::string& error);

// Applies every key in a preset object, e.g. { "decay": 0.8, "space": "MASSIVE" }.
bool applyPreset(Engine& engine, const json::Value& preset, std::string& error);

// Applies one iv_render `--set key=value` argument. Stricter than JSON,
// where the type tells a number from a label: knobs take a number 0-1;
// Switch3 params take only a label (space=MASSIVE, attack=BLOOM), so a bare
// number on a switch is an error rather than a silently wrong position.
bool applySetArg(Engine& engine, const std::string& arg, std::string& error);

} // namespace iv::paramsjson
