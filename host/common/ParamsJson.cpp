#include "ParamsJson.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace iv::paramsjson {

bool findParamId(const std::string& key, ParamId& id)
{
    for (const auto& p : kParams) {
        if (key == p.key) { id = p.id; return true; }
    }
    return false;
}

bool switchPosition(ParamId id, const std::string& label, int& position)
{
    const ParamSpec& s = spec(id);
    for (int pos = 0; pos < 3; ++pos) {
        if (s.choices[pos] && label == s.choices[pos]) { position = pos; return true; }
    }
    // Earlier labels still read (presets, sweeps and scripts written before the owner's names, 27 Oct 2026).
    if (id == ParamId::Attack && label == "NORMAL") { position = 0; return true; }
    // SPACE's right position was MASSIVE until v1.0.62 (ADR 0037): presets, sweeps and tools still say it.
    if (id == ParamId::Space && label == "MASSIVE") { position = 2; return true; }
    return false;
}

bool applyValue(Engine& engine, ParamId id, const json::Value& val, std::string& error)
{
    const ParamSpec& s = spec(id);
    if (val.isNumber()) {
        engine.setParam(id, float(val.numberValue()));
        return true;
    }
    if (val.isString() && s.kind == ParamKind::Switch3) {
        const std::string label = val.stringValue();
        int pos = 0;
        if (switchPosition(id, label, pos)) { engine.setParam(id, switchToNormalised(pos)); return true; }
        error = "unknown label '" + label + "' for " + s.key;
        return false;
    }
    error = std::string("bad value for ") + s.key;
    return false;
}

bool applyPreset(Engine& engine, const json::Value& preset, std::string& error)
{
    if (!preset.isObject()) { error = "preset must be a JSON object"; return false; }
    for (const auto& entry : preset.entries()) {
        ParamId id;
        if (!findParamId(entry.first, id)) { error = "unknown param '" + entry.first + "'"; return false; }
        if (!applyValue(engine, id, entry.second, error)) return false;
    }
    return true;
}

bool applySetArg(Engine& engine, const std::string& arg, std::string& error)
{
    const auto eq = arg.find('=');
    if (eq == std::string::npos) { error = "expected key=value"; return false; }
    const std::string key  = arg.substr(0, eq);
    const std::string text = arg.substr(eq + 1);
    ParamId id;
    if (!findParamId(key, id)) { error = "unknown param '" + key + "'"; return false; }
    const ParamSpec& s = spec(id);

    if (s.kind == ParamKind::Switch3) {
        int pos = 0;
        if (switchPosition(id, text, pos)) { engine.setParam(id, switchToNormalised(pos)); return true; }
        error = "'" + text + "' is not a position of " + s.key + " (use " + s.choices[0] + ", "
              + s.choices[1] + " or " + s.choices[2] + ")";
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(text.c_str(), &end);
    if (text.empty() || *end != '\0' || errno != 0 || !std::isfinite(value)) {
        error = std::string(s.key) + " takes a number 0-1, got '" + text + "'";
        return false;
    }
    engine.setParam(id, value);
    return true;
}

} // namespace iv::paramsjson
