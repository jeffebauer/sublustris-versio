// Renderer: offline CLI Host. WAV in -> Core -> WAV out (SPEC §6.2).
// M1 adds presets, automation, sweeps, metrics/sidecars and --analyze
// (docs/m1-contracts.md Stream B).
//
// Usage:
//   iv_render <in.wav> <out.wav> [--set key=value ...] [--preset p.json]
//             [--auto a.json] [--block N] [--sidecar]
//   iv_render --sweep sweep.json --out-dir DIR [--set key=value ...]
//   iv_render --analyze <in.wav> [--sidecar-out x.json] [--channel L|R|mix]
//   A sweep JSON may name an automation file
//   ("auto": "a.json", Automation.h), applied to every render (gesture renders).

#include "Automation.h"
#include "Json.h"
#include "Metrics.h"
#include "ParamsJson.h"
#include "Sidecar.h"
#include "Spectrogram.h"
#include "Sweep.h"
#include "Wav.h"
#include "dsp/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

using iv::wav::Audio;

void usage(const char* argv0)
{
    std::fprintf(stderr,
        "usage: %s <in.wav> <out.wav> [--set key=value ...] [--preset p.json] [--auto a.json] [--block N] [--sidecar]\n"
        "       %s --sweep sweep.json --out-dir DIR [--set key=value ...]\n"
        "       %s --analyze <in.wav> [--sidecar-out x.json] [--channel L|R|mix]\n",
        argv0, argv0, argv0);
}

// Mono downmix used by every analysis metric (sum of channels / channel
// count, matching the metrics contract).
std::vector<float> downmix(const Audio& a)
{
    const size_t frames = a.frames();
    std::vector<float> mono(frames, 0.0f);
    const size_t ch = a.channels.size();
    for (size_t i = 0; i < frames; ++i) {
        double sum = 0.0;
        for (size_t c = 0; c < ch; ++c) sum += double(a.channels[c][i]);
        mono[i] = ch ? float(sum / double(ch)) : 0.0f;
    }
    return mono;
}

std::string isoTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// Renders `in` through `engine`, applying automation at <=16-sample
// granularity: breakpoints re-interpolated at every micro-block boundary;
// gate and button events at their exact sample offset within the
// micro-block they land in.
Audio renderWithAutomation(iv::Engine& engine, const Audio& in, int block, const iv::automation::Automation* autom)
{
    const std::vector<float>& srcL = in.channels[0];
    const std::vector<float>& srcR = in.channels.size() > 1 ? in.channels[1] : in.channels[0];
    const size_t frames = in.frames();
    const float sr = float(in.sampleRate);

    // Output is always stereo, like the Versio's outputs and the Plugin:
    // the Engine is stereo even for a mono input.
    Audio out = in;
    out.channels.assign(2, std::vector<float>(frames));
    float* dstR = out.channels[1].data();

    std::vector<std::pair<size_t, bool>> gateSamples; // THROW (ADR 0039): sample-accurate gate changes
    if (autom)
        for (const auto& [t, high] : autom->gateEvents) gateSamples.emplace_back(size_t(std::lround(t * double(sr))), high);
    size_t nextGate = 0;
    std::vector<std::pair<size_t, bool>> buttonSamples; // the button (ADR 0043): sample-accurate presses and releases
    if (autom)
        for (const auto& [t, down] : autom->buttonEvents) buttonSamples.emplace_back(size_t(std::lround(t * double(sr))), down);
    size_t nextButton = 0;
    const int microBlock = autom ? std::min(block, 16) : block;
    for (size_t pos = 0; pos < frames; pos += size_t(microBlock)) {
        const int n = int(std::min<size_t>(size_t(microBlock), frames - pos));
        if (autom) {
            const double t = double(pos) / double(sr);
            for (const auto& track : autom->tracks)
                engine.setParam(track.key, float(iv::automation::valueAt(track, t)));
        }
        while (nextGate < gateSamples.size() && gateSamples[nextGate].first < pos + size_t(n)) {
            const size_t at = std::max(gateSamples[nextGate].first, pos);
            engine.gate(gateSamples[nextGate].second, int(at - pos));
            ++nextGate;
        }
        while (nextButton < buttonSamples.size() && buttonSamples[nextButton].first < pos + size_t(n)) {
            const size_t at = std::max(buttonSamples[nextButton].first, pos);
            engine.button(buttonSamples[nextButton].second, int(at - pos));
            ++nextButton;
        }
        engine.process(srcL.data() + pos, srcR.data() + pos, out.channels[0].data() + pos, dstR + pos, n);
    }
    return out;
}

// Builds an Engine prepared with defaults, applies preset then --set
// overrides. Returns false with `error` set on a bad preset/--set.
bool buildEngine(iv::Engine& engine, float sampleRate, int block, const std::string& presetPath,
               const std::vector<std::string>& sets, std::string& error)
{
    engine.prepare(sampleRate, block);
    // Prototype, desktop only: IV_REVERSE_MODE picks the REVERSE on trial (Engine::setReverseMode).
    if (const char* rm = std::getenv("IV_REVERSE_MODE")) engine.setReverseMode(std::atoi(rm));
    if (!presetPath.empty()) {
        iv::json::Value preset;
        if (!iv::json::loadFile(presetPath, preset, error)) return false;
        if (!iv::paramsjson::applyPreset(engine, preset, error)) return false;
    }
    for (const auto& s : sets) {
        if (!iv::paramsjson::applySetArg(engine, s, error)) { error = "bad --set " + s + ": " + error; return false; }
    }
    engine.reset(); // start at the settings, no glide in from the defaults
    return true;
}

// Analyzes `mono` and prints the one-line metrics summary. Returns the
// computed metrics + spectrogram for sidecar writing.
void analyzeAndReport(const Audio& audio, const std::vector<float>& mono, iv::metrics::Metrics& outM,
                       iv::spectrogram::Spectrogram& outSpec)
{
    outM = iv::metrics::compute(audio.channels, float(audio.sampleRate));
    outSpec = iv::spectrogram::compute(mono, float(audio.sampleRate));
    std::printf("%s\n", iv::metrics::summaryLine(outM).c_str());
}

int runAnalyze(const std::string& inPath, const std::string& sidecarOut, const std::string& channel)
{
    Audio in;
    std::string error;
    if (!iv::wav::read(inPath, in, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }

    std::vector<float> mono;
    if (channel == "L") mono = in.channels[0];
    else if (channel == "R") mono = in.channels.size() > 1 ? in.channels[1] : in.channels[0];
    else mono = downmix(in);

    // Metrics use the requested channel selection directly (as `mono`
    // stands in for the whole file here: pass it as a single-channel set
    // so nan/inf + clip counts and every analysis metric read that
    // selection, matching --channel L|R|mix). The stereo metrics
    // (docs/m4-contracts.md Stream E) are unaffected by --channel: pass
    // the file's real channels so they measure its actual stereo image
    // (e.g. a stereo wet take analyzed with --channel L still reports a
    // real stereo_correlation, not "mono").
    std::vector<std::vector<float>> single{mono};
    iv::metrics::Metrics m = iv::metrics::compute(single, float(in.sampleRate), &in.channels);
    iv::spectrogram::Spectrogram spec = iv::spectrogram::compute(mono, float(in.sampleRate));
    std::printf("%s\n", iv::metrics::summaryLine(m).c_str());

    if (!sidecarOut.empty()) {
        iv::json::Value params = iv::json::Value::makeObject(); // no Engine params for a raw analysis
        const double durationS = double(in.frames()) / double(in.sampleRate);
        iv::json::Value side = iv::sidecar::build(inPath, in.sampleRate, durationS, params, m, spec);
        if (!iv::json::saveFile(sidecarOut, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
        std::printf("sidecar -> %s\n", sidecarOut.c_str());
    }
    return 0;
}

int runSweep(const std::string& sweepPath, const std::string& outDir, const std::vector<std::string>& sets)
{
    std::string error;
    iv::sweep::Config cfg;
    if (!iv::sweep::loadFile(sweepPath, cfg, error)) { std::fprintf(stderr, "sweep: %s\n", error.c_str()); return 1; }

    Audio srcIn;
    if (!iv::wav::read(cfg.input, srcIn, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }

    // Append tail_seconds of silence so tails ring out.
    Audio tailed = srcIn;
    const size_t tailFrames = size_t(cfg.tailSeconds * double(srcIn.sampleRate));
    for (auto& ch : tailed.channels) ch.resize(ch.size() + tailFrames, 0.0f);

    // Optional automation (a gesture: TONE swept on a ringing tail...), the
    // same for every render of the sweep.
    iv::automation::Automation sweepAuto;
    if (!cfg.autoPath.empty() && !iv::automation::loadFile(cfg.autoPath, sweepAuto, error)) {
        std::fprintf(stderr, "sweep auto: %s\n", error.c_str());
        return 1;
    }

    std::system(("mkdir -p \"" + outDir + "\"").c_str());

    iv::json::Value manifest = iv::json::Value::makeObject();
    manifest.set("name", iv::json::Value::makeString(cfg.name));
    manifest.set("created", iv::json::Value::makeString(isoTimestamp()));
    manifest.set("input", iv::json::Value::makeString(cfg.input));
    manifest.set("ignore_flags", cfg.ignoreFlags);
    if (!cfg.autoPath.empty()) manifest.set("auto", iv::json::Value::makeString(cfg.autoPath));
    iv::json::Value renders = iv::json::Value::makeArray();

    const auto combos = iv::sweep::cartesian(cfg.grid);
    for (const auto& combo : combos) {
        iv::Engine engine;
        engine.prepare(float(tailed.sampleRate), 48);
        if (!iv::paramsjson::applyPreset(engine, cfg.base, error)) {
            std::fprintf(stderr, "sweep base preset: %s\n", error.c_str());
            return 1;
        }
        for (const auto& s : sets) {
            std::string err;
            if (!iv::paramsjson::applySetArg(engine, s, err)) { std::fprintf(stderr, "sweep: bad --set %s: %s\n", s.c_str(), err.c_str()); return 1; }
        }
        for (const auto& [key, value] : combo) {
            iv::ParamId id;
            if (!iv::paramsjson::findParamId(key, id)) { std::fprintf(stderr, "sweep: unknown grid key '%s'\n", key.c_str()); return 1; }
            engine.setParam(id, float(value));
        }
        engine.reset(); // start at the grid point, no glide in from the defaults

        const std::string base = iv::sweep::fileBaseName(cfg.name, combo);
        const std::string wavName = base + ".wav";
        const std::string sidecarName = base + ".json";
        const std::string wavPath = outDir + "/" + wavName;
        const std::string sidecarPath = outDir + "/" + sidecarName;

        Audio out = renderWithAutomation(engine, tailed, 48, cfg.autoPath.empty() ? nullptr : &sweepAuto);
        if (!iv::wav::write(wavPath, out, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }

        std::vector<float> mono = downmix(out);
        iv::metrics::Metrics m; iv::spectrogram::Spectrogram spec;
        analyzeAndReport(out, mono, m, spec);

        const iv::json::Value params = iv::sidecar::paramsToJson(engine);
        const double durationS = double(out.frames()) / double(out.sampleRate);
        iv::json::Value side = iv::sidecar::build(wavName, out.sampleRate, durationS, params, m, spec);
        if (!iv::json::saveFile(sidecarPath, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }

        iv::json::Value entry = iv::json::Value::makeObject();
        entry.set("wav", iv::json::Value::makeString(wavName));
        entry.set("sidecar", iv::json::Value::makeString(sidecarName));
        entry.set("params", params);
        renders.push(entry);
        std::printf("rendered %s\n", wavName.c_str());
    }
    manifest.set("renders", renders);
    const std::string manifestPath = outDir + "/manifest.json";
    if (!iv::json::saveFile(manifestPath, manifest, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
    std::printf("manifest -> %s\n", manifestPath.c_str());
    return 0;
}

int runRender(const std::string& inPath, const std::string& outPath, int block, const std::string& presetPath,
              const std::string& autoPath, const std::vector<std::string>& sets, bool writeSidecar)
{
    Audio in;
    std::string error;
    if (!iv::wav::read(inPath, in, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }
    if (in.channels.size() > 2) { std::fprintf(stderr, "read: only mono or stereo input\n"); return 1; }

    iv::Engine engine;
    if (!buildEngine(engine, float(in.sampleRate), block, presetPath, sets, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }

    iv::automation::Automation autom;
    const iv::automation::Automation* automPtr = nullptr;
    if (!autoPath.empty()) {
        if (!iv::automation::loadFile(autoPath, autom, error)) { std::fprintf(stderr, "auto: %s\n", error.c_str()); return 2; }
        automPtr = &autom;
    }

    Audio out = renderWithAutomation(engine, in, block, automPtr);
    if (!iv::wav::write(outPath, out, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
    std::printf("rendered %zu frames @ %d Hz -> %s\n", out.frames(), in.sampleRate, outPath.c_str());

    std::vector<float> mono = downmix(out);
    iv::metrics::Metrics m; iv::spectrogram::Spectrogram spec;
    analyzeAndReport(out, mono, m, spec);

    if (writeSidecar) {
        const iv::json::Value params = iv::sidecar::paramsToJson(engine);
        const double durationS = double(out.frames()) / double(out.sampleRate);
        // Sidecar file name mirrors the WAV stem, next to it.
        std::string stem = outPath;
        const auto dot = stem.find_last_of('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        const std::string sidecarPath = stem + ".json";
        const auto slash = outPath.find_last_of('/');
        const std::string wavBase = slash == std::string::npos ? outPath : outPath.substr(slash + 1);
        iv::json::Value side = iv::sidecar::build(wavBase, out.sampleRate, durationS, params, m, spec);
        if (!iv::json::saveFile(sidecarPath, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
        std::printf("sidecar -> %s\n", sidecarPath.c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { usage(argv[0]); return 2; }

    const std::string first = argv[1];
    if (first == "--sweep") {
        std::string sweepPath, outDir;
        std::vector<std::string> sets;
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--out-dir" && i + 1 < argc) outDir = argv[++i];
            else if (a == "--set" && i + 1 < argc) sets.push_back(argv[++i]);
            else sweepPath = a;
        }
        if (sweepPath.empty() || outDir.empty()) { usage(argv[0]); return 2; }
        return runSweep(sweepPath, outDir, sets);
    }

    if (first == "--analyze") {
        if (argc < 3) { usage(argv[0]); return 2; }
        const std::string inPath = argv[2];
        std::string sidecarOut, channel = "mix";
        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--sidecar-out" && i + 1 < argc) sidecarOut = argv[++i];
            else if (a == "--channel" && i + 1 < argc) channel = argv[++i];
            else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
        }
        if (channel != "L" && channel != "R" && channel != "mix") { std::fprintf(stderr, "--channel must be L, R or mix\n"); return 2; }
        return runAnalyze(inPath, sidecarOut, channel);
    }

    if (argc < 3) { usage(argv[0]); return 2; }
    const std::string inPath = argv[1], outPath = argv[2];
    int block = 48; // matches the Versio's default block size
    std::string presetPath, autoPath;
    bool writeSidecar = false;
    std::vector<std::string> sets;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--set" && i + 1 < argc) sets.push_back(argv[++i]);
        else if (a == "--block" && i + 1 < argc) block = std::atoi(argv[++i]);
        else if (a == "--preset" && i + 1 < argc) presetPath = argv[++i];
        else if (a == "--auto" && i + 1 < argc) autoPath = argv[++i];
        else if (a == "--sidecar") writeSidecar = true;
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }
    if (block < 1) block = 1;

    return runRender(inPath, outPath, block, presetPath, autoPath, sets, writeSidecar);
}
