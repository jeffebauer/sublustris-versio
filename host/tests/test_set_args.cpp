// iv_render `--set key=value` parsing (iv::paramsjson::applySetArg).
// Seeded from Resilio, where every value once went through strtof, so a
// switch label silently became the wrong position. Checks labels, numbers
// and rejections in-process, then runs the real iv_render binary to prove
// --set space=MASSIVE renders the same as a --preset with "space": "MASSIVE".

#include "Json.h"
#include "ParamsJson.h"
#include "Wav.h"
#include "dsp/Engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

bool set(iv::Engine& engine, const char* arg)
{
    std::string error;
    return iv::paramsjson::applySetArg(engine, arg, error);
}

void labelsAndNumbers()
{
    iv::Engine engine;
    engine.prepare(48000.0f, 48);
    check(set(engine, "space=PLATE") && engine.param(iv::ParamId::Space) == 0.0f, "space=PLATE sets the left position");
    check(set(engine, "space=HALL") && engine.param(iv::ParamId::Space) == 0.5f, "space=HALL sets the centre position");
    check(set(engine, "space=MASSIVE") && engine.param(iv::ParamId::Space) == 1.0f, "space=MASSIVE sets the right position");
    check(set(engine, "attack=NORMAL") && engine.param(iv::ParamId::Attack) == 0.0f, "attack=NORMAL sets the left position");
    check(set(engine, "attack=BLOOM") && engine.param(iv::ParamId::Attack) == 0.5f, "attack=BLOOM sets the centre position");
    check(set(engine, "attack=REVERSE") && engine.param(iv::ParamId::Attack) == 1.0f, "attack=REVERSE sets the right position");
    check(set(engine, "decay=0.8") && std::fabs(engine.param(iv::ParamId::Decay) - 0.8f) < 1e-6f, "numeric knob value accepted");

    iv::Engine fresh;
    fresh.prepare(48000.0f, 48);
    std::string error;
    check(!iv::paramsjson::applySetArg(fresh, "space=massive", error), "label is case-sensitive: 'massive' rejected");
    check(!set(fresh, "space=CAVE"), "unknown label rejected");
    check(!set(fresh, "space=1"), "switch given a bare number rejected");
    check(!set(fresh, "attack=0.5"), "switch given a Normalised number rejected");
    check(!set(fresh, "decay=BLOOM"), "knob given text rejected");
    check(!set(fresh, "decay=0.8x"), "knob given trailing junk rejected");
    check(!set(fresh, "decay="), "knob given nothing rejected");
    check(!set(fresh, "decay=nan"), "knob given nan rejected");
    check(!set(fresh, "desk=0.5"), "unknown key rejected (DESK was removed, ADR 0015)");
    check(!set(fresh, "decay"), "missing '=' rejected");
    check(fresh.param(iv::ParamId::Space) == 0.5f, "rejected values leave the param at its default");
}

bool readWav(const std::string& path, iv::wav::Audio& a)
{
    std::string error;
    return iv::wav::read(path, a, error);
}

bool sameAudio(const iv::wav::Audio& a, const iv::wav::Audio& b)
{
    if (a.channels.size() != b.channels.size() || a.frames() != b.frames()) return false;
    for (size_t c = 0; c < a.channels.size(); ++c)
        for (size_t i = 0; i < a.frames(); ++i)
            if (a.channels[c][i] != b.channels[c][i]) return false;
    return true;
}

int run(const std::string& args)
{
    const std::string cmd = "./iv_render " + args + " > /dev/null 2>&1";
    return std::system(cmd.c_str());
}

// End to end through the iv_render binary (the test runs in the build dir).
void cliMatchesPreset()
{
    const std::string dir = "test_set_args_tmp";
    std::system(("mkdir -p " + dir).c_str());

    // Loud decaying noise hits.
    iv::wav::Audio in;
    in.sampleRate = 48000;
    in.channels.assign(1, std::vector<float>(48000, 0.0f));
    uint32_t seed = 1;
    for (size_t i = 0; i < in.frames(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float noise = float(int32_t(seed)) / 2147483648.0f;
        const size_t t = i % 12000;
        in.channels[0][i] = t < 2400 ? 0.9f * noise * std::exp(-float(t) / 600.0f) : 0.0f;
    }
    std::string error;
    const std::string inPath = dir + "/in.wav";
    check(iv::wav::write(inPath, in, error), "cli: stimulus written");

    std::ofstream(dir + "/massive.json") << R"({ "space": "MASSIVE", "attack": "BLOOM", "decay": 0.9 })";

    check(run(inPath + " " + dir + "/set.wav --set space=MASSIVE --set attack=BLOOM --set decay=0.9") == 0, "cli: --set with labels exits 0");
    check(run(inPath + " " + dir + "/preset.wav --preset " + dir + "/massive.json") == 0, "cli: --preset exits 0");

    iv::wav::Audio viaSet, viaPreset;
    const bool read = readWav(dir + "/set.wav", viaSet) && readWav(dir + "/preset.wav", viaPreset);
    check(read, "cli: renders read back");
    if (read) check(sameAudio(viaSet, viaPreset), "cli: --set space=MASSIVE renders identically to the --preset");

    // SPEC §7 M0: at MIX 0 the Renderer copies a WAV through unchanged, bit
    // for bit, to both outputs (a mono input feeds both, as the In L normal).
    check(run(inPath + " " + dir + "/pass.wav --set mix=0") == 0, "cli: mix=0 render exits 0");
    iv::wav::Audio pass, written; // compare with the stimulus as written (quantised to the file's bits)
    bool same = readWav(dir + "/pass.wav", pass) && readWav(inPath, written) && pass.channels.size() == 2
             && pass.frames() == written.frames();
    for (size_t c = 0; same && c < 2; ++c)
        for (size_t i = 0; same && i < written.frames(); ++i) same = pass.channels[c][i] == written.channels[0][i];
    check(same, "cli: MIX 0 passthrough is bit-identical on both outputs");

    check(run(inPath + " " + dir + "/bad.wav --set space=CAVE") != 0, "cli: unknown label exits non-zero");
    check(run(inPath + " " + dir + "/bad.wav --set space=1") != 0, "cli: bare number on a switch exits non-zero");
    check(run(inPath + " " + dir + "/bad.wav --set decay=high") != 0, "cli: text on a knob exits non-zero");

    std::system(("rm -rf " + dir).c_str());
}

} // namespace

int main()
{
    labelsAndNumbers();
    cliMatchesPreset();
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
