// Plugin Host's panel (seeded from Resilio's, ADR 0001): not a custom design,
// just JUCE's standard knobs, buttons and text placed where the Versio's
// parts sit, plus the four LED meters running the release firmware's own
// meter maths and freeze tint (firmware/LedMeter.h, SPEC §3 LEDs) so a
// clipping input, a limiting output or a frozen pad shows at the desk the
// way it does on the module.

#include "PluginEditor.h"

#include "KnobText.h"

#include "../firmware/LedMeter.h"
#include "params/ParamSpec.h"

#include <cmath>

namespace iv::plugin {

namespace {

// ---- Panel geometry, in mm from the panel's top-left corner ----------------
// Copied from tools/make_panel_svg.py (Noise Engineering's official Versio
// template, in Resilio); the pot -> function order is ADR 0017's.
constexpr float kPanelW   = 50.5f;  // 10 HP as cut
constexpr float kPxPerMm  = 5.0f;   // at 1x: 253 x 643 px
constexpr int   kWidthPx  = 253;
constexpr int   kHeightPx = 643;

struct Part {
    float x, y; // centre, mm
};

struct Knob {
    ParamId id;
    Part    at;
    bool    labelAbove; // P6: the toggles sit below it
};

constexpr Knob kKnobs[] = {
    {ParamId::Mix,   {7.770f, 18.530f}, false},  // P1
    {ParamId::Decay, {43.330f, 18.530f}, false}, // P2
    {ParamId::Tone,  {25.169f, 28.690f}, false}, // P3
    {ParamId::Era,   {7.770f, 39.485f}, false},  // P4
    {ParamId::Size,  {43.330f, 39.485f}, false}, // P5
    {ParamId::Drift, {25.169f, 49.328f}, true},  // P6
    {ParamId::Time,  {43.330f, 60.440f}, false}, // P7
};

// Top toggle = SPACE (SW0), the one below = ATTACK (SW1); orientation
// confirmed at the M0 hardware check.
// The hardware toggles throw left / centre / right: choices[0..2].
struct Toggle {
    ParamId id;
    Part    at;
};
constexpr Toggle kToggles[] = {
    {ParamId::Space,  {8.405f, 57.900f}}, // SW0
    {ParamId::Attack, {8.405f, 67.425f}}, // SW1
};

// The button sits at the module's height (69.33 mm), nudged right of its
// 26.185 mm centre and drawn wider than the 7 mm cap, so FREEZE reads in
// full at 1x and still clears ATTACK's SHIMMER.
constexpr Part kButton{28.0f, 69.330f};
// The plugin's own ZOOM control (panel scale) in the bottom row.
constexpr float kBottomRowYMm = 121.0f;
constexpr float kBottomRowHMm = 4.0f;

// LED1..LED4, left to right: In L, In R, Out L, Out R (PanelLink::Meter order).
constexpr Part kLeds[PanelLink::kNumMeters] = {
    {17.295f, 19.800f}, {22.375f, 19.800f}, {29.360f, 19.800f}, {34.440f, 19.800f}};

constexpr float kKnobSizeMm    = 12.5f; // slider bounds; the cap on the module is ~11.6 mm
constexpr float kLedRadiusMm   = 1.8f;  // the hole is 3 mm
constexpr float kSegmentWMm    = 6.8f;  // one toggle position (three side by side)
constexpr float kSegmentHMm    = 4.5f;
constexpr float kSegmentLeftMm = 1.5f;  // the three end left of the button
constexpr float kButtonWMm     = 10.5f;
constexpr float kButtonHMm     = 7.0f;
constexpr float kLabelHMm      = 2.6f;

constexpr float px(float mm) { return mm * kPxPerMm; }

juce::Rectangle<int> centred(Part at, float wMm, float hMm)
{
    return juce::Rectangle<float>(px(wMm), px(hMm)).withCentre({px(at.x), px(at.y)}).toNearestInt();
}

// ---- Colours ----------------------------------------------------------------
const juce::Colour kPanelColour{0xff26282b};
const juce::Colour kOutlineColour{0xff6b6f75};
const juce::Colour kTextColour{0xffe6e6e6};
const juce::Colour kDimTextColour{0xff9a9ea4};

// An unlit LED's body (linear light, before the sRGB encode below).
constexpr float kLedBodyLight = 0.012f;

float srgbEncode(float linear)
{
    const float v = std::clamp(linear, 0.0f, 1.0f);
    return v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// LedMeter.h gives drive values; on the module the PWM cubes each channel
// (ivled::pwmCount) and the LED's light follows that duty. A screen pixel's
// value is gamma-encoded, so the same light needs sRGB-encoding the duty:
// a dim LED is as dim on screen as on the panel (kMinGlow -> 2.7 % light).
juce::Colour ledColour(const ivled::Rgb& drive)
{
    auto channel = [](float d) {
        const float duty = float(ivled::pwmCount(d)) / float(ivled::kPwmSteps);
        return srgbEncode(kLedBodyLight + duty);
    };
    return juce::Colour::fromFloatRGBA(channel(drive.r), channel(drive.g), channel(drive.b), 1.0f);
}


// JUCE's standard look with three tweaks for a dark panel: the value arc
// and the chosen toggle position in the accent colour (the dark scheme's
// defaults are nearly invisible here), knobs drawn about the size of the
// module's caps, and smaller button text so CLEAN / TAPE / VALVE fit a
// toggle position about 7 mm wide. A knob marked "bipolar" (DRIFT) fills
// its arc from noon, not from the left end.
const juce::Identifier kBipolar{"bipolar"};

struct PanelLook final : juce::LookAndFeel_V4 {
    PanelLook()
    {
        const auto accent = findColour(juce::Slider::thumbColourId);
        setColour(juce::Slider::rotarySliderFillColourId, accent);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions(std::min(11.0f, float(buttonHeight) * 0.45f), juce::Font::bold);
    }

    // LookAndFeel_V4::drawRotarySlider, with a smaller inset and the bipolar fill.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float startAngle,
                          float endAngle, juce::Slider& slider) override
    {
        const auto bounds    = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto centre    = bounds.getCentre();
        const float radius   = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float toAngle  = startAngle + sliderPos * (endAngle - startAngle);
        const float lineW    = std::min(7.0f, radius * 0.3f);
        const float arcR     = radius - lineW * 0.5f;
        const bool  bipolar  = bool(slider.getProperties().getWithDefault(kBipolar, false));
        const float fromAngle = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
        const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(track, stroke);

        if (slider.isEnabled() && std::abs(toAngle - fromAngle) > 0.01f) {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(fromAngle, toAngle),
                                std::max(fromAngle, toAngle), true);
            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(value, stroke);
        }

        const float thumbW = lineW * 2.0f;
        const juce::Point<float> thumb(centre.x + arcR * std::cos(toAngle - juce::MathConstants<float>::halfPi),
                                       centre.y + arcR * std::sin(toAngle - juce::MathConstants<float>::halfPi));
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(juce::Rectangle<float>(thumbW, thumbW).withCentre(thumb));
    }
};

} // namespace

// ---- The panel ----------------------------------------------------------------
// Laid out at 1x (5 px per mm); the editor scales it as a whole.
class PanelEditor::Panel final : public juce::Component, private juce::Timer {
public:
    Panel(juce::AudioProcessorValueTreeState& state, PanelLink& link, std::function<void(float)> onScale,
          float scale)
        : link_(link)
    {
        setLookAndFeel(&lookAndFeel_);

        for (size_t i = 0; i < knobs_.size(); ++i) {
            const auto id = kKnobs[i].id;
            auto& s       = knobs_[i];
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setPopupDisplayEnabled(true, true, this);
            s.getProperties().set(kBipolar, id == ParamId::Drift);
            addAndMakeVisible(s);
            // The attachment also sets double-click -> the ParamSpec default.
            knobAttach_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, spec(id).key, s);
            s.textFromValueFunction = [id](double v) { return knobText(id, v); };
        }

        for (size_t t = 0; t < toggles_.size(); ++t) {
            const auto& ps = spec(kToggles[t].id);
            auto*       p  = state.getParameter(ps.key);
            for (int pos = 0; pos < 3; ++pos) {
                auto& b = toggles_[t][static_cast<size_t>(pos)];
                b.setButtonText(ps.choices[pos]);
                b.setClickingTogglesState(true);
                b.setRadioGroupId(int(t) + 1, juce::dontSendNotification);
                b.setConnectedEdges((pos > 0 ? juce::Button::ConnectedOnLeft : 0)
                                    | (pos < 2 ? juce::Button::ConnectedOnRight : 0));
                b.onClick = [this, t, pos] {
                    if (toggles_[t][static_cast<size_t>(pos)].getToggleState())
                        toggleAttach_[t]->setValueAsCompleteGesture(float(pos));
                };
                addAndMakeVisible(b);
            }
            toggleAttach_[t] = std::make_unique<juce::ParameterAttachment>(*p, [this, t](float v) {
                const int pos = std::clamp(juce::roundToInt(v), 0, 2);
                toggles_[t][static_cast<size_t>(pos)].setToggleState(true, juce::dontSendNotification);
            });
            toggleAttach_[t]->sendInitialUpdate();
        }

        // FREEZE, as on the module (ADR 0009–0011): a tap latches the
        // freeze, a hold while frozen paints. Every press and release goes
        // to the audio thread, which hands it to the Engine at the start of
        // its next block; the Engine's gesture reader does the rest.
        button_.setButtonText("FREEZE");
        button_.onStateChange = [this] {
            const bool down = button_.isDown();
            if (down == buttonDown_) return;
            buttonDown_ = down;
            if (down) link_.button.press();
            else link_.button.release();
        };
        addAndMakeVisible(button_);

        const float scales[3] = {1.0f, 1.5f, 2.0f};
        const char* names[3]  = {"1x", "1.5x", "2x"};
        for (size_t i = 0; i < sizes_.size(); ++i) {
            auto& b = sizes_[i];
            b.setButtonText(names[i]);
            b.setClickingTogglesState(true);
            b.setRadioGroupId(10, juce::dontSendNotification);
            b.setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
            b.setToggleState(juce::approximatelyEqual(scale, scales[i]), juce::dontSendNotification);
            b.onClick = [this, onScale, s = scales[i], i] {
                if (sizes_[i].getToggleState()) onScale(s);
            };
            addAndMakeVisible(b);
        }

        setSize(kWidthPx, kHeightPx);
        lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60);
    }

    ~Panel() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void resized() override
    {
        for (size_t i = 0; i < knobs_.size(); ++i)
            knobs_[i].setBounds(centred(kKnobs[i].at, kKnobSizeMm, kKnobSizeMm));

        for (size_t t = 0; t < toggles_.size(); ++t)
            for (size_t pos = 0; pos < 3; ++pos) {
                const Part c{kSegmentLeftMm + kSegmentWMm * (float(pos) + 0.5f), kToggles[t].at.y};
                toggles_[t][pos].setBounds(centred(c, kSegmentWMm, kSegmentHMm));
            }

        button_.setBounds(centred(kButton, kButtonWMm, kButtonHMm));

        // The bottom row: ZOOM left-aligned with SPACE and ATTACK.
        // Start on SPACE's rounded left edge (7.5 px rounds to 8) so the three
        // 35 px segments land on whole pixels and butt together.
        constexpr float sizeW = 7.0f;
        const float sizeLeftMm = float(juce::roundToInt(px(kSegmentLeftMm))) / kPxPerMm;
        for (size_t i = 0; i < sizes_.size(); ++i)
            sizes_[i].setBounds(centred({sizeLeftMm + sizeW * (float(i) + 0.5f), kBottomRowYMm}, sizeW, kBottomRowHMm));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kPanelColour);
        g.setColour(kOutlineColour);
        g.drawRect(getLocalBounds(), 1);

        auto text = [&g](const juce::String& s, Part at, float hMm, float sizePx, juce::Colour c,
                         juce::Justification j = juce::Justification::centred, float wMm = 16.0f) {
            g.setColour(c);
            g.setFont(juce::FontOptions(sizePx, juce::Font::bold));
            g.drawFittedText(s, centred(at, wMm, hMm), j, 1, 0.8f);
        };

        text("IMMENSIO VERSIO", {kPanelW * 0.5f, 9.0f}, 3.0f, 12.0f, kTextColour, juce::Justification::centred, 30.0f);

        // Meter labels: left pair is the input, right pair the output.
        text("IN", {(kLeds[0].x + kLeds[1].x) * 0.5f, 15.6f}, kLabelHMm, 9.0f, kDimTextColour);
        text("OUT", {(kLeds[2].x + kLeds[3].x) * 0.5f, 15.6f}, kLabelHMm, 9.0f, kDimTextColour);

        for (const auto& k : kKnobs) {
            const float half = kKnobSizeMm * 0.5f;
            const float y    = k.labelAbove ? k.at.y - half - 2.6f : k.at.y + half + 1.6f;
            text(spec(k.id).name, {k.at.x, y}, kLabelHMm, 11.0f, kTextColour);
        }

        // DRIFT is bipolar (ADR 0012): a tick at noon, where it is still.
        {
            const auto& w   = kKnobs[5];
            const float top = w.at.y - kKnobSizeMm * 0.5f;
            g.setColour(kTextColour);
            g.drawLine(px(w.at.x), px(top - 1.0f), px(w.at.x), px(top + 0.3f), 1.5f); // ends above the arc
        }

        for (const auto& t : kToggles)
            text(spec(t.id).name, {kSegmentLeftMm + 7.0f, t.at.y - kSegmentHMm * 0.5f - 1.6f}, kLabelHMm, 11.0f,
                 kTextColour, juce::Justification::centredLeft, 14.0f);

        text("ZOOM", {kSegmentLeftMm + 7.0f, 117.0f}, kLabelHMm, 9.0f, kDimTextColour, juce::Justification::centredLeft, 14.0f);

        for (int m = 0; m < PanelLink::kNumMeters; ++m) {
            const auto  c = kLeds[m];
            const float r = px(kLedRadiusMm);
            const auto  e = juce::Rectangle<float>(2.0f * r, 2.0f * r).withCentre({px(c.x), px(c.y)});
            // The output pair shows the freeze state under its meter (SPEC §3 LEDs).
            auto c3 = meters_[static_cast<size_t>(m)].colour();
            if (m >= PanelLink::kOutL)
                c3 = ivled::withFreezeTint(c3, link_.frozen(), link_.painting(), float(lastTickMs_ * 0.001));
            g.setColour(ledColour(c3));
            g.fillEllipse(e);
            g.setColour(kOutlineColour);
            g.drawEllipse(e, 1.0f);
        }
    }

private:
    // Same steps as firmware/main.cpp's main loop, at ~60 Hz with the real
    // elapsed time, so ballistics and red hold match the module.
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float  dt  = float(std::max(0.0, now - lastTickMs_) * 0.001);
        lastTickMs_      = now;

        float peak[PanelLink::kNumMeters];
        for (int m = 0; m < PanelLink::kNumMeters; ++m) peak[m] = link_.takePeak(m);

        // Red: input near full scale; output while the Engine's limiter
        // pulls the wet down (stereo-linked: both output LEDs together).
        const bool limiting = ivled::limiterReducing(link_.takeLimiterGain());
        meters_[PanelLink::kInL].update(peak[PanelLink::kInL], ivled::inputNearClip(peak[PanelLink::kInL]), dt);
        meters_[PanelLink::kInR].update(peak[PanelLink::kInR], ivled::inputNearClip(peak[PanelLink::kInR]), dt);
        meters_[PanelLink::kOutL].update(peak[PanelLink::kOutL], limiting, dt);
        meters_[PanelLink::kOutR].update(peak[PanelLink::kOutR], limiting, dt);

        repaint(juce::Rectangle<float>(px(kLeds[0].x - 2.5f), px(kLeds[0].y - 2.5f),
                                       px(kLeds[3].x - kLeds[0].x + 5.0f), px(5.0f))
                    .toNearestInt());
    }

    PanelLink&      link_;
    PanelLook       lookAndFeel_;

    std::array<juce::Slider, std::size(kKnobs)> knobs_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, std::size(kKnobs)> knobAttach_;
    std::array<std::array<juce::TextButton, 3>, std::size(kToggles)> toggles_;
    std::array<std::unique_ptr<juce::ParameterAttachment>, std::size(kToggles)> toggleAttach_;
    juce::TextButton                button_;
    bool                            buttonDown_ = false;
    std::array<juce::TextButton, 3> sizes_;

    std::array<ivled::LevelMeter, PanelLink::kNumMeters> meters_;
    double                                               lastTickMs_ = 0.0;
};

// ---- The editor: the panel at the chosen size --------------------------------
namespace {
// The chosen size is kept in the plugin's saved state (not a parameter, so no
// automation lane and nothing for the DSP to see).
const juce::Identifier kScaleProperty{"panelScale"};
} // namespace

PanelEditor::PanelEditor(juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& state, PanelLink& link)
    : AudioProcessorEditor(owner), state_(state)
{
    float scale = float(double(state_.state.getProperty(kScaleProperty, 1.0)));
    if (!(juce::approximatelyEqual(scale, 1.5f) || juce::approximatelyEqual(scale, 2.0f))) scale = 1.0f;

    panel_ = std::make_unique<Panel>(state, link, [this](float s) {
        state_.state.setProperty(kScaleProperty, s, nullptr);
        setPanelScale(s);
    }, scale);
    addAndMakeVisible(*panel_);
    setResizable(false, false);
    setPanelScale(scale);
}

PanelEditor::~PanelEditor() = default;

void PanelEditor::setPanelScale(float scale)
{
    panel_->setTransform(juce::AffineTransform::scale(scale));
    setSize(juce::roundToInt(float(kWidthPx) * scale), juce::roundToInt(float(kHeightPx) * scale));
}

void PanelEditor::resized() { panel_->setTopLeftPosition(0, 0); }

} // namespace iv::plugin
