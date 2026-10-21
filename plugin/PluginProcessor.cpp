// Plugin Host. Parameters are generated from the ParamSpec table; this
// file never lists parameters by hand (SPEC §6).

#include "PluginEditor.h"
#include "dsp/Engine.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : iv::kParams) {
        const juce::ParameterID id{p.key, 1};
        if (p.kind == iv::ParamKind::Switch3) {
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                id, p.name, juce::StringArray{p.choices[0], p.choices[1], p.choices[2]},
                iv::normalisedToSwitch(p.defaultValue)));
        } else {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(0.0f, 1.0f), p.defaultValue));
        }
    }
    return layout;
}

class ImmensioVersioProcessor final : public juce::AudioProcessor {
public:
    ImmensioVersioProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state_(*this, nullptr, "params", makeLayout())
    {
        for (const auto& p : iv::kParams)
            raw_[static_cast<size_t>(p.id)] = state_.getRawParameterValue(p.key);
    }

    void prepareToPlay(double sampleRate, int maxBlock) override
    {
        engine_.prepare(float(sampleRate), maxBlock);
        pushParams();
        engine_.reset(); // start at the knobs' current settings, no glide in from defaults
        midiGate_.reset();
        setLatencySamples(0); // reported to the host once the Core adds latency (M2 criterion)
    }

    void releaseResources() override {}

    void pushParams()
    {
        for (const auto& p : iv::kParams) {
            const float v = raw_[static_cast<size_t>(p.id)]->load();
            engine_.setParam(p.id, p.kind == iv::ParamKind::Switch3 ? iv::switchToNormalised(int(v)) : v);
        }
    }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::stereo()
            && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;

        pushParams();

        const int n = buffer.getNumSamples();

        // The panel's FREEZE button (ButtonLink.h): its presses and releases
        // since the last block, at this block's start.
        panel_.button.feed(engine_, n);
        // MIDI notes are the gate (MidiGate.h): held = high, sample-accurate.
        for (const auto m : midi) {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) midiGate_.noteOn(engine_, msg.getNoteNumber(), m.samplePosition);
            else if (msg.isNoteOff()) midiGate_.noteOff(engine_, msg.getNoteNumber(), m.samplePosition);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) midiGate_.allOff(engine_, m.samplePosition);
        }
        const float* inL = buffer.getReadPointer(0);
        const float* inR = getTotalNumInputChannels() > 1 ? buffer.getReadPointer(1) : inL; // mono in -> both sides

        // LED meters (as the firmware's audio callback takes them): abs peaks
        // of the input before the Engine and the output after it, and the
        // limiter's gain. The panel reads them.
        using Link = iv::plugin::PanelLink;
        panel_.notePeak(Link::kInL, peakOf(inL, n));
        panel_.notePeak(Link::kInR, peakOf(inR, n));
        engine_.process(inL, inR, buffer.getWritePointer(0), buffer.getWritePointer(1), n);
        panel_.notePeak(Link::kOutL, peakOf(buffer.getReadPointer(0), n));
        panel_.notePeak(Link::kOutR, peakOf(buffer.getReadPointer(1), n));
        panel_.noteLimiterGain(engine_.limiterGain());
        panel_.noteEngineState(engine_.frozen(), engine_.painting());
    }

    juce::AudioProcessorEditor* createEditor() override { return new iv::plugin::PanelEditor(*this, state_, panel_); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120.0; } // max Massive T60 (SPEC §3)

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        if (auto xml = state_.copyState().createXml())
            copyXmlToBinary(*xml, dest);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
            state_.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    static float peakOf(const float* x, int n)
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i) p = std::max(p, std::abs(x[i]));
        return p;
    }

    iv::plugin::PanelLink panel_; // the button and the LED meters, shared with the editor
    juce::AudioProcessorValueTreeState state_;
    std::array<std::atomic<float>*, static_cast<size_t>(iv::ParamId::Count)> raw_{};
    iv::Engine engine_;
    iv::plugin::MidiGate midiGate_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImmensioVersioProcessor)
};

} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ImmensioVersioProcessor(); }
