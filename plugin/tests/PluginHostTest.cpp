// Plugin host test bench (SPEC §7 M2), seeded from Resilio's (ADR 0001).
// Loads the *built* AU/VST3 bundles the way a DAW does —
// juce::AudioPluginFormatManager over the headless plugin hosting module (no
// GUI needed to load and process a plugin) — and checks them against SPEC §7
// M2. MIDI notes are the gate (MidiGate.h) and the panel's FREEZE button goes
// through ButtonLink.h; both are JUCE-free and checked here against a
// recording engine, edge by edge and sample by sample.
//
// Bundle paths come from CMake (IV_VST3_PATH / IV_AU_PATH, generated from
// $<TARGET_BUNDLE_DIR:...> so they always point at the just-built artefact).

#include "dsp/Engine.h"
#include "params/ParamSpec.h"
#include "../ButtonLink.h"
#include "../MidiGate.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

void note(const std::string& what) { std::printf("NOTE  %s\n", what.c_str()); }

// ---------------------------------------------------------------------------
// Plugin loading helpers
// ---------------------------------------------------------------------------

std::unique_ptr<juce::AudioPluginInstance> loadPlugin(juce::AudioPluginFormatManager& fm,
                                                       const juce::String& path,
                                                       double sampleRate, int blockSize,
                                                       juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* format : fm.getFormats()) {
        if (format->fileMightContainThisPluginType(path)) {
            format->findAllTypesForFile(found, path);
            if (!found.isEmpty()) break;
        }
    }
    if (found.isEmpty()) { error = "no plugin types found at " + path; return nullptr; }
    return fm.createPluginInstance(*found[0], sampleRate, blockSize, error);
}

// VST3 (and classic AU) expose only a numeric parameter ID to the host, never
// our ParamSpec `key`. What is visible and stable is declaration order and
// the display name, and PluginProcessor.cpp's makeLayout() emits parameters
// in exactly iv::kParams order, so hosted parameters are identified by
// position, and every name is checked against ParamSpec at that position.
// JUCE's VST3 wrapper appends one synthetic "Bypass" parameter after ours.
using ParamMap = std::array<juce::AudioProcessorParameter*, size_t(iv::ParamId::Count)>;

ParamMap mapParams(juce::AudioPluginInstance& inst, bool* orderAndNamesOk = nullptr)
{
    ParamMap byId{};
    auto& params = inst.getParameters();
    bool ok = params.size() >= int(iv::kParams.size());
    for (size_t i = 0; i < iv::kParams.size(); ++i) {
        auto* r = (int(i) < params.size()) ? params[int(i)] : nullptr;
        byId[i] = r;
        if (!r || r->getName(64) != juce::String(iv::spec(iv::ParamId(i)).name)) ok = false;
    }
    if (orderAndNamesOk) *orderAndNamesOk = ok;
    return byId;
}

// A setting off every default, for the parity and null tests (Normalised,
// switches as their Normalised position). MIX is the caller's.
struct Setting {
    iv::ParamId id;
    float       v;
};
constexpr Setting kPreset[] = {
    {iv::ParamId::Decay, 0.8f}, {iv::ParamId::Tone, 0.3f}, {iv::ParamId::Era, 0.7f}, {iv::ParamId::Size, 0.9f},
    {iv::ParamId::Drift, 0.2f}, {iv::ParamId::Time, 0.6f}, {iv::ParamId::Space, 1.0f}, {iv::ParamId::Attack, 0.5f},
};

void setPresetOnPlugin(const ParamMap& byId, float mix)
{
    for (const auto& s : kPreset)
        if (auto* p = byId[size_t(s.id)]) p->setValueNotifyingHost(s.v);
    if (auto* p = byId[size_t(iv::ParamId::Mix)]) p->setValueNotifyingHost(mix);
}

void setPresetOnEngine(iv::Engine& engine, float mix)
{
    for (const auto& s : kPreset) engine.setParam(s.id, s.v);
    engine.setParam(iv::ParamId::Mix, mix);
}

// Block-size schedules a DAW might send, covering a signal of `total` samples.
std::vector<int> blockSchedule(const std::vector<int>& pattern, int total)
{
    std::vector<int> out;
    size_t i = 0;
    int done = 0;
    while (done < total) {
        int n = std::min(pattern[i % pattern.size()], total - done);
        out.push_back(n);
        done += n;
        ++i;
    }
    return out;
}

double maxAbsDiffDb(const std::vector<float>& a, const std::vector<float>& b)
{
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m == 0.0f ? -400.0 : 20.0 * std::log10(double(m));
}

// Stimulus, generated here (deterministic): clicks, decaying noise bursts
// and a low sine, two seconds at 48 kHz, peaks under -6 dBFS.
std::vector<float> makeStimulus()
{
    constexpr int kFs = 48000;
    std::vector<float> x(size_t(2 * kFs), 0.0f);
    uint32_t s = 0x1234567u;
    auto noise = [&s] { s = s * 1664525u + 1013904223u; return float(int32_t(s)) * (1.0f / 2147483648.0f); };
    for (int k = 0; k < 4; ++k) x[size_t(k * kFs / 2 + 100)] = 0.5f; // clicks
    for (int k = 0; k < 3; ++k) {                                    // bursts
        const int start = k * kFs / 2 + kFs / 8;
        for (int i = 0; i < kFs / 10; ++i) x[size_t(start + i)] += 0.4f * noise() * std::exp(-float(i) / 1200.0f);
    }
    for (size_t i = 0; i < x.size(); ++i) x[i] += 0.05f * std::sin(2.0f * 3.14159265f * 110.0f * float(i) / kFs);
    return x;
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

struct Render { std::vector<float> outL, outR; };

// Engine-direct render: mono input duplicated to L/R (as PluginProcessor's
// mono-in handling); the gate high from gateOn to gateOff (-1 = never).
Render renderEngine(const std::vector<float>& in, const std::vector<int>& blocks, float mix, int gateOn = -1,
                    int gateOff = -1)
{
    iv::Engine engine;
    engine.prepare(48000.0f, 512);
    setPresetOnEngine(engine, mix);
    engine.reset(); // as the plugin's prepareToPlay: start at the settings, no glide
    Render r;
    r.outL.assign(in.size(), 0.0f);
    r.outR.assign(in.size(), 0.0f);
    int pos = 0;
    for (int n : blocks) {
        if (gateOn >= pos && gateOn < pos + n) engine.gate(true, gateOn - pos);
        if (gateOff >= pos && gateOff < pos + n) engine.gate(false, gateOff - pos);
        engine.process(in.data() + pos, in.data() + pos, r.outL.data() + pos, r.outR.data() + pos, n);
        pos += n;
    }
    return r;
}

// Plugin-hosted render over the same schedule. `midiNote` >= 0: a note-on
// at gateOn and its note-off at gateOff, with junk (a note-off of a note
// never played, a CC) that must do nothing.
Render renderPlugin(juce::AudioPluginFormatManager& fm, const juce::String& path, const std::vector<float>& in,
                    const std::vector<int>& blocks, float mix, int gateOn = -1, int midiNote = -1, int gateOff = -1)
{
    juce::String error;
    auto inst = loadPlugin(fm, path, 48000.0, 512, error);
    Render r;
    if (!inst) { check(false, "plugin loads for render: " + error.toStdString()); return r; }
    // A DAW restores the saved settings, then prepares the plugin.
    setPresetOnPlugin(mapParams(*inst), mix);
    inst->prepareToPlay(48000.0, 512);

    r.outL.assign(in.size(), 0.0f);
    r.outR.assign(in.size(), 0.0f);
    int pos = 0;
    for (int n : blocks) {
        juce::AudioBuffer<float> buffer(2, n);
        buffer.copyFrom(0, 0, in.data() + pos, n);
        buffer.copyFrom(1, 0, in.data() + pos, n);

        juce::MidiBuffer midi;
        if (midiNote >= 0 && gateOn >= pos && gateOn < pos + n)
            midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, juce::uint8(90)), gateOn - pos);
        if (midiNote >= 0 && gateOff >= pos && gateOff < pos + n)
            midi.addEvent(juce::MidiMessage::noteOff(1, midiNote), gateOff - pos);
        midi.addEvent(juce::MidiMessage::noteOff(1, 37), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);

        inst->processBlock(buffer, midi);
        std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + n, r.outL.data() + pos);
        std::copy(buffer.getReadPointer(1), buffer.getReadPointer(1) + n, r.outR.data() + pos);
        pos += n;
    }
    return r;
}

// Records every edge the Engine would see, as (kind, high, absolute sample).
struct EdgeRecorder {
    struct Edge {
        char kind; // 'b' button, 'g' gate
        bool on;
        long at;
        bool operator==(const Edge& o) const { return kind == o.kind && on == o.on && at == o.at; }
    };
    std::vector<Edge> edges;
    long blockStart = 0;
    void button(bool pressed, int offset) { edges.push_back({'b', pressed, blockStart + offset}); }
    void gate(bool high, int offset) { edges.push_back({'g', high, blockStart + offset}); }
};

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit; // starts the JUCE MessageManager (needed to host plugins)

    juce::AudioPluginFormatManager fm;
    juce::addHeadlessDefaultFormatsToManager(fm);

    const juce::String vst3Path = IV_VST3_PATH;
#ifdef IV_AU_PATH
    const juce::String auPath = IV_AU_PATH;
    const bool haveAuPath = true;
#else
    const juce::String auPath;
    const bool haveAuPath = false;
#endif

    // ---- 1. Loads at 44.1/48/96 kHz, stereo in/out and mono in/stereo out ----
    {
        struct Cfg { double sr; bool monoIn; };
        const Cfg cfgs[] = {{44100.0, false}, {44100.0, true}, {48000.0, false}, {48000.0, true},
                            {96000.0, false}, {96000.0, true}};
        struct Fmt { const char* name; juce::String path; bool available; bool strict; };
        const Fmt formats[] = {{"VST3", vst3Path, true, true}, {"AU", auPath, haveAuPath, false}};
        for (const auto& fmt : formats) {
            if (!fmt.available) { note("AU bundle path not configured (non-Apple build?)"); continue; }
            for (const auto& c : cfgs) {
                juce::String error;
                auto inst = loadPlugin(fm, fmt.path, c.sr, 512, error);
                bool ok = inst != nullptr;
                if (ok && c.monoIn) {
                    juce::AudioProcessor::BusesLayout layout;
                    layout.inputBuses.add(juce::AudioChannelSet::mono());
                    layout.outputBuses.add(juce::AudioChannelSet::stereo());
                    ok = inst->setBusesLayout(layout) && inst->checkBusesLayoutSupported(layout);
                }
                if (ok) inst->prepareToPlay(c.sr, 512);

                std::string what = std::string(fmt.name) + " loads at " + std::to_string(int(c.sr)) + " Hz, "
                                  + (c.monoIn ? "mono" : "stereo") + " in/stereo out";
                if (!ok) what += " (" + error.toStdString() + ")";
                if (fmt.strict) check(ok, what);
                else if (ok) check(ok, what);
                else note(what + " -- best-effort AU hosting (an unregistered AU may not load), not a hard failure");

                if (inst) inst->releaseResources();
            }
        }
    }

    // ---- 2. Parameters: generated from iv::kParams, nothing hand-written ----
    {
        juce::String error;
        auto inst = loadPlugin(fm, vst3Path, 48000.0, 512, error);
        if (!inst) {
            check(false, "VST3 loads for parameter check: " + error.toStdString());
        } else {
            check(size_t(inst->getParameters().size()) == iv::kParams.size() + 1,
                  "hosted parameter count == iv::kParams.size() + JUCE's VST3 Bypass ("
                      + std::to_string(inst->getParameters().size()) + ")");
            bool orderOk = false;
            const auto byId = mapParams(*inst, &orderOk);
            check(orderOk, "hosted parameters, in order, have exactly iv::kParams' names");
            for (const auto& p : iv::kParams) {
                auto* param = byId[size_t(p.id)];
                const std::string what = std::string("param ") + p.name;
                if (!param) { check(false, what + " present at its ParamSpec position"); continue; }
                check(param->isAutomatable(), what + " is automatable");
                if (p.kind == iv::ParamKind::Knob) {
                    check(std::fabs(param->getDefaultValue() - p.defaultValue) < 1e-6f, what + " default matches ParamSpec");
                    check(param->getNumSteps() > 1000, what + " is continuous (Knob)");
                } else {
                    // A 3-choice param's Normalised default is its index / 2.
                    check(std::fabs(param->getDefaultValue() - p.defaultValue) < 1e-6f, what + " default matches ParamSpec");
                    check(param->getNumSteps() == 3, what + " has exactly 3 choices (Switch3)");
                    bool labelsMatch = true;
                    for (int pos = 0; pos < 3; ++pos)
                        if (param->getText(iv::switchToNormalised(pos), 64) != juce::String(p.choices[pos])) labelsMatch = false;
                    check(labelsMatch, what + " choice labels match ParamSpec");
                }
            }
        }
    }

    const std::vector<float> stimulus = makeStimulus();
    const int total = int(stimulus.size());

    // ---- 3. Plugin == direct Engine render, any block schedule, with the gate ----
    {
        const std::vector<std::vector<int>> patterns = {{512}, {64}, {1}, {37, 512, 1, 300, 129}};
        const int gateOn = 24011, gateOff = 61003;
        for (const auto& pattern : patterns) {
            const auto blocks = blockSchedule(pattern, total);
            const auto direct = renderEngine(stimulus, blocks, 0.7f, gateOn, gateOff);
            const auto hosted = renderPlugin(fm, vst3Path, stimulus, blocks, 0.7f, gateOn, 60, gateOff);
            if (hosted.outL.empty()) continue;
            const bool same = hosted.outL == direct.outL && hosted.outR == direct.outR;
            std::string sched;
            for (int b : pattern) sched += std::to_string(b) + " ";
            check(same, "VST3 render == Engine render, bit-identical, MIX 0.7 + gate via MIDI, blocks [" + sched
                            + "] (max diff L " + std::to_string(maxAbsDiffDb(hosted.outL, direct.outL)) + " dB)");
        }
    }

    // ---- 4. Latency and null test at MIX 0 ----
    {
        juce::String error;
        auto inst = loadPlugin(fm, vst3Path, 48000.0, 512, error);
        if (!inst) {
            check(false, "VST3 loads for latency/null-test: " + error.toStdString());
        } else {
            inst->prepareToPlay(48000.0, 512);
            check(inst->getLatencySamples() == 0, "getLatencySamples() == 0");
        }
        const auto blocks = blockSchedule({256}, total);
        const auto hosted = renderPlugin(fm, vst3Path, stimulus, blocks, 0.0f);
        if (!hosted.outL.empty())
            check(hosted.outL == stimulus && hosted.outR == stimulus,
                  "MIX 0 nulls against the dry input, bit-identical (the dry path is untouched, SPEC §4.10)");
    }

    // ---- 5. MIDI gate (MidiGate.h): edges reach Engine::gate sample-accurately ----
    {
        EdgeRecorder rec;
        iv::plugin::MidiGate gate;
        // Block 0 (512): note 60 on at 17, note 62 on at 40 (no new edge),
        // note 37 off (never played: nothing), note 60 off at 100 (62 still
        // held: nothing). Block 1: note 62 off at 5 (falling edge), on at 6.
        // Block 2: all notes off at 300.
        rec.blockStart = 0;
        gate.noteOn(rec, 60, 17);
        gate.noteOn(rec, 62, 40);
        gate.noteOff(rec, 37, 50);
        gate.noteOff(rec, 60, 100);
        rec.blockStart = 512;
        gate.noteOff(rec, 62, 5);
        gate.noteOn(rec, 64, 6);
        rec.blockStart = 1024;
        gate.allOff(rec, 300);
        gate.allOff(rec, 301); // already low: nothing
        const std::vector<EdgeRecorder::Edge> want = {{'g', true, 17}, {'g', false, 517}, {'g', true, 518}, {'g', false, 1324}};
        check(rec.edges == want, "MIDI notes: gate high while any note is held, each edge at its exact sample");

        // The Engine itself records the gate's level (Engine::gateHigh()),
        // once the block holding the edge has been processed.
        iv::Engine engine;
        engine.prepare(48000.0f, 64);
        std::vector<float> z(64, 0.0f), ol(64), orr(64);
        iv::plugin::MidiGate g2;
        g2.noteOn(engine, 48, 3);
        engine.process(z.data(), z.data(), ol.data(), orr.data(), 64);
        const bool high = engine.gateHigh();
        g2.noteOff(engine, 48, 9);
        engine.process(z.data(), z.data(), ol.data(), orr.data(), 64);
        check(high && !engine.gateHigh(), "MIDI note on/off reach Engine::gate() (gateHigh() follows)");
    }

    // ---- 6. FREEZE button (ButtonLink.h): presses reach Engine::button ----
    {
        EdgeRecorder rec;
        iv::plugin::ButtonLink link;
        const int n = 64;
        auto block = [&](long start) { rec.blockStart = start; link.feed(rec, n); };
        block(0);                       // nothing
        link.press(); block(64);        // press at the block start
        block(128);                     // held: nothing new
        link.release(); block(192);     // release at the block start
        link.press(); link.release();   // a whole click between blocks
        block(256);                     // press at 256, release at its last sample
        link.press(); block(320);       // down
        link.release(); link.press();   // let go and pressed again between blocks
        block(384);                     // release at 384, press at 385
        const std::vector<EdgeRecorder::Edge> want = {{'b', true, 64}, {'b', false, 192}, {'b', true, 256},
                                                      {'b', false, 319}, {'b', true, 320}, {'b', false, 384},
                                                      {'b', true, 385}};
        check(rec.edges == want, "panel button: presses and releases reach Engine::button() at block starts; "
                                 "a click shorter than a block is still a press + release");
        iv::Engine engine;
        engine.prepare(48000.0f, 64);
        iv::plugin::ButtonLink l2;
        l2.press();
        l2.feed(engine, 64);
        std::vector<float> z(64, 0.0f), ol(64), orr(64);
        engine.process(z.data(), z.data(), ol.data(), orr.data(), 64);
        check(engine.buttonDown() && engine.frozen(), "panel button reaches Engine::button() (buttonDown() follows, a press freezes)");
    }

    // ---- 7. State save / restore ----
    {
        juce::String e1, e2;
        auto a = loadPlugin(fm, vst3Path, 48000.0, 512, e1);
        auto b = loadPlugin(fm, vst3Path, 48000.0, 512, e2);
        if (!a || !b) {
            check(false, "two VST3 instances load for state test");
        } else {
            const auto pa = mapParams(*a);
            setPresetOnPlugin(pa, 0.37f);
            juce::MemoryBlock state;
            a->getStateInformation(state);
            check(state.getSize() > 0, "getStateInformation produced non-empty state");
            b->setStateInformation(state.getData(), int(state.getSize()));
            const auto pb = mapParams(*b);
            bool allMatch = true;
            for (size_t i = 0; i < iv::kParams.size(); ++i)
                if (!pa[i] || !pb[i] || std::fabs(pa[i]->getValue() - pb[i]->getValue()) > 1e-6f) allMatch = false;
            check(allMatch, "setStateInformation on a new instance restores every parameter value");
        }
    }

    std::printf("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
