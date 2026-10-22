#pragma once
// Renderer metrics (docs/m1-contracts.md Stream B, SPEC §4.10 / §6.2, plus
// the stereo metrics added by docs/m4-contracts.md Stream E).
// All analysis metrics (t60, resonance, steady_tone, click_count) run on
// the mono downmix (sum of channels / channel count); nan_inf_count and
// clip_count scan every channel. peak/rms are also over the mono downmix
// per the contract. Use NaN in t60S / resonancePeakDb to mean "null" /
// not measurable (written as JSON null).
//
// Stereo metrics (stereoCorrelation, monoLossDb, monoNotchDb) are NaN
// ("null" / "mono" in the sidecar and review page) whenever the file has
// fewer than 2 channels -- they need a real L/R pair. maxStepDb100ms is a
// mono metric like t60/resonance and is always attempted regardless of
// channel count.

#include <cmath>
#include <string>
#include <vector>

namespace iv::metrics {

struct Metrics {
    double peakDbfs        = -200.0;
    double rmsDbfs          = -200.0;
    double t60S             = std::nan("");
    double resonancePeakDb  = std::nan("");
    bool   steadyTone       = false;
    long   nanInfCount      = 0;
    long   clipCount        = 0;
    long   clickCount       = 0;
    double stereoCorrelation = std::nan(""); // Pearson r of L/R over the T60 segment; null for mono
    double monoLossDb        = std::nan(""); // 10*log10(power(L+R) / (power(L)+power(R))); null for mono
    double monoNotchDb       = std::nan(""); // deepest mono-sum-vs-stereo-avg dip, 200Hz-5kHz; null for mono
    double maxStepDb100ms    = std::nan(""); // largest |RMS dB| step between consecutive 100ms windows

    // ---- M6 Ringing metric (docs/m6-metric-calibration.md) ----------------
    // "Does one narrow frequency keep outliving its neighbours?" Over the
    // tail (after the input stopped), each FFT bin's prominence above its
    // 1/3-octave neighbourhood median is followed in time. ringingDb = the
    // largest of (a) the dB a narrow peak keeps gaining on its neighbourhood
    // over the late half of the tail, and (b) for a steady tone (a peak that
    // hardly decays, >= 20 dB clear, for >= 2 s) its prominence; capped at
    // 70 dB ("a bare tone"). A spring's own modes all decay together, so
    // however peaky its spectrum is, ringingDb stays low; a Ringing mode
    // climbs out. NaN = not measurable (tail too short). Metrics.cpp has the
    // step-by-step definition.
    double ringingDb         = std::nan("");
    double ringingHz         = std::nan(""); // frequency of that bin
    double ringingRatio      = std::nan(""); // neighbourhood decay rate / that bin's decay rate (late half)
    double ringingEndDb      = std::nan(""); // its prominence at the end of the span, dB
    double ringingSpanS      = std::nan(""); // longest late-half span analysed, seconds
    bool   ringing           = false;        // ringingDb >= kRingingGrowthDb

    // ---- M6 Howl criterion (ADR 0019), for the KICKED Howl zone only ------
    // Over the sustained part (1 s after the first event to the end of the file):
    // howlFloorDb = median 1/3-octave band (200 Hz-5 kHz) re the strongest
    // band (>= -25: noise/harmonics present, not a bare sine).
    // howlMovePct / howlMoveDb: the *steadiest* 2 s window's strongest-peak
    // frequency spread (%) and level spread (dB); moving = every window
    // with an audible peak (> -30 dBFS) has >= 0.5 % or >= 3 dB.
    double howlFloorDb       = std::nan("");
    double howlMovePct       = std::nan("");
    double howlMoveDb        = std::nan("");
    bool   howlOk            = false;        // floor and movement both pass

    // ---- Room-acoustics measures (docs/research/reverb-testing-practices.md
    // items 8, 11, 12). All are measured on the first event's segment (its
    // onset to the next event, or to the end of the file), like t60S.
    // "Onset" is ISO 3382-1:2009's start of the impulse response: the first
    // sample of the mono downmix that rises to 20 dB below the segment's
    // peak. "Tail end" is where the 50 ms envelope has fallen
    // 50 dB below its loudest point (or the segment's end). Metrics.cpp has
    // the step-by-step definitions and sources. NaN = not measurable.
    //
    // Normalized echo density (Abel and Huang 2006): 1 = Gaussian-noise-like
    // ("a smooth wash"), well below 1 = separate echoes ("grainy").
    double echoDensityTimeS  = std::nan(""); // onset -> NED (100 ms mean) first >= 0.9 and holds 100 ms; worse side
    double echoDensityLate   = std::nan(""); // mean NED from 0.3 s after the input stops to the tail end; worse side
    // Decay per octave band, 125 Hz-8 kHz (bandHz), with the same octave
    // filters and decay fit as tools/ingest_reference.py, so renders and
    // the owner's recordings of reference units compare like for like.
    static constexpr int kNumBands = 7;
    static constexpr double bandHz[kNumBands] = {125, 250, 500, 1000, 2000, 4000, 8000};
    double bandT60S[kNumBands] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN}; // ingest_reference.py's t60()
    double bandEdtS[kNumBands] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN}; // ISO 3382-1 EDT (0 to -10 dB fit, x6)
    double c80Db             = std::nan(""); // ISO 3382-1 clarity: energy 0-80 ms vs after, dB (mono)
    double iaccEarly         = std::nan(""); // ISO 3382-1 IACC over 0-80 ms (max |IACF|, +-1 ms); null for mono
    double iaccLate          = std::nan(""); // ... over 80 ms to the tail end; null for mono
    double tailFlatnessDb    = std::nan(""); // RMS dB deviation of the Schroeder curve from its -5..-35 dB line fit
};

// Echo-density thresholds for echoDensityTimeS (the "grainy" check).
constexpr double kEchoDensityTarget = 0.9;
constexpr double kEchoDensityHoldS  = 0.1;

// Calibrated thresholds (docs/m6-metric-calibration.md).
constexpr double kRingingGrowthDb        = 15.0;
constexpr double kRingingEndProminenceDb = 6.0;
constexpr double kRingingMinSpanS        = 0.5;  // late-half span needed to measure
constexpr double kRingingSpanDb          = 80.0; // analyse each neighbourhood's first 80 dB of decay
constexpr double kRingingSmoothS         = 0.35; // time smoothing of each bin's level
constexpr double kRingingMinT60Ratio     = 0.5;  // a growing peak must itself last >= this x the tail's T60
constexpr double kRingingSteadyDbPerS    = 3.0;  // steady tone: decays slower than this (T60 > 20 s) ...
constexpr double kRingingSteadyS         = 2.0;  // ... for at least this long ...
constexpr double kRingingSteadyProminenceDb = 20.0; // ... standing this far above its neighbourhood
constexpr double kRingingSteadyRangeDb   = 40.0; // ... and no more than this below the tail's start level
constexpr double kRingingLeakDb          = 70.0; // growth stops counting here: the reference is the peak's own window leakage
// ADR 0034 round 2 (WOBBLE moves modes by about a bin): a bin's level is
// the max over +-this many bins (+-12 Hz at 48 kHz), and a climb must rise
// in both halves of its late half (each third-to-third step >= this share
// of the whole rise, 1 dB slack).
constexpr size_t kRingingFollowBins      = 2;
constexpr double kRingingSteadyShare     = 0.2;
constexpr double kHowlFloorMinDb         = -25.0;
constexpr double kHowlMovePct            = 0.5;
constexpr double kHowlMoveDb             = 3.0;

// `channels` is [channel][frame], any channel count >= 1. The stereo
// metrics (stereoCorrelation/monoLossDb/monoNotchDb) read `channels[0]`
// and `channels[1]` directly when `channels.size() >= 2`, unless
// `stereoSource` is given, in which case they read `(*stereoSource)[0]`
// and `[1]` instead -- this lets a caller downmix/select a channel for
// the classic mono-based metrics (e.g. --analyze's --channel L|R|mix)
// while still measuring the file's real stereo image.
Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate,
                 const std::vector<std::vector<float>>* stereoSource = nullptr);

// The echo density profile itself (exposed for tests): normalized echo
// density of `x` with a `windowS` Hann window centred every `hop` samples
// over [start, end). Element k is centred at start + k * hop.
std::vector<double> echoDensityProfile(const std::vector<float>& x, size_t start, size_t end, float sampleRate,
                                       size_t hop, double windowS = 0.020);

// One-line human summary printed to stdout per render.
std::string summaryLine(const Metrics& m);

} // namespace iv::metrics
