#pragma once
// The Engine: everything between the inputs and the outputs (CONTEXT.md:
// big tank, twin, loop chain, Era, output stage). Every Host drives it
// the same way: prepare(), setParam() with Normalised values, button() and
// gate() edges at a sample offset in the next block, process().
//
// M5: the three SPACEs (PLATE on the Plate, HALL and MASSIVE on the FDN),
// ATTACK, and the freeze: In → predelay → input diffusion → swell → big
// tank → MIX (SPEC §4.1); in SHIMMER the diffused input, shifted an octave up, joins the input into
// the tank (dsp/Shimmer.h, ADR 0024). The button and the gate feed one
// gesture reader (ADR 0011): a tap freezes the big tank into a still pad and
// moves the input to the twin (PLATE: a second Plate; HALL and MASSIVE: a
// small room, v1.0.43) whose sound fades back out over the pad (SIZE, DECAY
// and TONE play it); holding while frozen
// paints a little of the twin into the pad (ADR 0010); a tap unfreezes and
// the big tank glides back to the knobs (ADR 0009).
//
// Memory (SPEC §5.2): the firmware calls attachMemory() with its SDRAM and
// DTCM arrays before prepare(). Desktop Hosts may skip it: the Engine then
// owns vectors (not in the firmware, which builds with IV_NO_HEAP).
#include "dsp/Diffuser.h"
#include "dsp/EraTexture.h"
#include "dsp/Fdn.h"
#include "dsp/Memory.h"
#include "dsp/Output.h"
#include "dsp/HalfRate.h"
#include "dsp/Plate.h"
#include "dsp/Shimmer.h"
#include "dsp/SizeOpt.h"
#include "dsp/Swell.h"
#include "dsp/Transport.h"
#include "dsp/TwinRoom.h"
#include "params/GestureReader.h"
#include "params/ParamSpec.h"
#include "params/SpaceVoicing.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#if !defined(IV_NO_HEAP)
#include <vector>
#endif

namespace iv {

class Engine {
public:
    // Output ceiling of the soft limiter (SPEC §4.10), linear.
    static constexpr float kLimitThreshold = 0.891f; // −1 dBFS

    // Floats of each memory kind the Engine needs at this sample rate.
    static constexpr size_t largeFloats(float sampleRate)
    {
        return dsp::Fdn::memoryFloats(0.5f * sampleRate) + 2 * dsp::Swell::memoryFloats(sampleRate) // the tank (at half the rate), the tank's swell and the twin's
             + 2 * dsp::Plate::memoryFloats(sampleRate, voicing::kPlateMaxScale, true)
             + dsp::Fdn::memoryFloats(sampleRate / float(kLowDecimate)) // still lows' low tank
             + 2 * dsp::nextPow2(size_t(voicing::kPredelayMaxSeconds * sampleRate) + 4)
             + dsp::Arena::skewRoom() // each line's start staggered across the cache's sets
             - axiTankFloats(sampleRate); // the tank's and the low tank's allpasses: axiFloats()
    }
    // The in-loop allpass lines of the tank and the low tank (not the twin's), which the firmware keeps
    // in AXI SRAM (overnight review, 26 Oct 2026): on chip and cached, ~11 cycles a line fill against
    // SDRAM's ~200. 192 + 96 KB at 48 kHz (each line's first allpass in a ring half the second's since
    // 28 Oct 2026), + the cache-set stagger. The twin's (PLATE) stay in SDRAM.
    static constexpr size_t axiTankFloats(float sampleRate)
    {
        return dsp::Fdn::apFloats(0.5f * sampleRate) + dsp::Fdn::apFloats(sampleRate / float(kLowDecimate));
    }
    // AXI SRAM: the allpass lines, then the SHIMMER's right ring (CPU hunt, 28 Oct 2026: off SDRAM).
    static constexpr size_t axiFloats(float sampleRate)
    {
        // The stagger (dsp/Memory.h): the first slice needs up to kSkewLines - 1 lines of padding; when
        // every slice is a whole number of cache ways (4 KB: the allpass lines at 24 and 12 kHz), each
        // later one needs exactly kSkewStep lines; otherwise up to kSkewLines - 1 each.
        constexpr size_t way = dsp::Arena::kSkewLines * dsp::Arena::kLineFloats, slices = 2 * dsp::Fdn::kApSlices;
        const bool whole = dsp::Fdn::apLineFloats(0.5f * sampleRate, 0) % way == 0 && dsp::Fdn::apLineFloats(0.5f * sampleRate, 1) % way == 0
                        && dsp::Fdn::apLineFloats(sampleRate / float(kLowDecimate), 0) % way == 0
                        && dsp::Fdn::apLineFloats(sampleRate / float(kLowDecimate), 1) % way == 0;
        const size_t padLines = whole ? (dsp::Arena::kSkewLines - 1) + (slices - 1) * dsp::Arena::kSkewStep
                                      : slices * (dsp::Arena::kSkewLines - 1);
        return axiTankFloats(sampleRate) + padLines * dsp::Arena::kLineFloats
             + dsp::Shimmer::ringFloats(sampleRate) + (dsp::Arena::kSkewLines - 1) * dsp::Arena::kLineFloats;
    }
    // D2 SRAM (the firmware's 256 KB of cached SRAM1-3; CPU hunt, 28 Oct 2026): the SHIMMER's left ring and
    // the twin's small room (v1.0.43, in AXI SRAM until then), one slice each.
    static constexpr size_t d2Floats(float sampleRate)
    {
        return dsp::Shimmer::ringFloats(sampleRate) + dsp::TwinRoom::memoryFloats(0.5f * sampleRate)
             + 2 * (dsp::Arena::kSkewLines - 1) * dsp::Arena::kLineFloats;
    }
    static constexpr size_t fastFloats(float sampleRate)
    {
        size_t n = 0;
        for (int twin = 0; twin < 2; ++twin) // the input diffusers, and the twin's (13 % longer)
            for (int c = 0; c < 2; ++c)
                for (float s : voicing::kDiffuserSeconds)
                    n += dsp::nextPow2(size_t(s * (c ? voicing::kDiffuserRightScale : 1.0f) * (twin ? 1.13f : 1.0f) * sampleRate) + 4);
        return n;
    }

    // axi: the in-loop allpass lines' memory and a SHIMMER ring (axiFloats(); the firmware's AXI SRAM); d2: the
    // other ring and the small room (d2Floats(); its D2 SRAM). Desktop Hosts may skip them too. With IV_NO_HEAP
    // and none attached, prepare() finds no room and the Engine stays dry.
    void attachMemory(float* large, size_t largeCount, float* fast, size_t fastCount, float* axi = nullptr,
                      size_t axiCount = 0, float* d2 = nullptr, size_t d2Count = 0);

    void prepare(float sampleRate, int maxBlockSize);
    void reset();

    void setParam(ParamId id, float normalised);
    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Button press / release at a sample offset within the next process() block.
    void button(bool pressed, int sampleOffset) { queueEdge(true, pressed, sampleOffset); }
    // Gate edge: high = a press, low = a release (ADR 0011). Unpatched reads low.
    void gate(bool high, int sampleOffset) { queueEdge(false, high, sampleOffset); }

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    bool  frozen() const { return gesture_.frozen(); }
    bool  painting() const { return gesture_.painting(); }
    // Test hook: scales the tanks' own modulation (0 = none; DRIFT is separate), to check a
    // structure isn't metallic without it (Costello: tune it to sound good unmodulated).
    void  setModulationScale(float k) { modScale_ = k; }
    // Test hook: how much of DECAY's level make-up applies (1 = v1.0.28; 0 = none, shorter tails
    // quieter, as v1.0.27), for demos.
    void  setDecayMakeup(float k) { decayMakeup_ = k; }
    // Test hook: false = one tank, the lows moving too (before v1.0.18), for demos.
    void  setStillLows(bool on)
    {
        lowTank_ = on;
        fdn_.setExternalPower(0.0f);
    }
    // PROTOTYPE (Mutable Instruments idea 4, no measurable gain; kept for a later listen): the
    // right input diffuser gets its own lengths instead of a 1.071x copy of the left.
    void  setDiffuserIndependent(bool on);
    // Test/demo override of HALL's in-loop diffusion amount (default kHallInLoopDiffusion, v1.0.17;
    // 0..1 × kInLoopCoeff, fading out by SIZE noon).
    void  setHallInLoopDiffusion(float a) { hallInLoop_ = a; }
#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (renders/fit_cpu's D, held back: "reduce the early reflections to 1 instead of 3 if
    // needed"): the tanks with fewer early taps per line. Re-prepares the Engine.
    void  setEarlyTapPrototype(int earlyTaps);
    // PROTOTYPE (renders/swell_seed, owner: "the 12 points seemed to accentuate the mids"): the swell's
    // jitter and signs from another seed (0 = as built). Re-prepares the Engine.
    void  setSwellSeedPrototype(uint32_t seed);
    // SIZE around FREEZE and SPACE (v1.0.80; ADR 0056-0058), each so two SIZE-driven jobs never run on the module at once:
    // - the room makes way (ADR 0056, ufsSize / ufsRamp): unfrozen in HALL and CANYON, while the small room rings out after an
    //   unfreeze, a SIZE off the one the pad froze with waits; the room fades out in 0.3 s, then the latest SIZE lands once;
    // - the room keeps its size (ADR 0056, applySpace): the small room plays the SIZE the pad froze with, never the live knob;
    // - SIZE waits for the morph (ADR 0057, ffHold): during a PLATE <-> HALL / CANYON morph the new tank keeps the SIZE it
    //   started with until the old tank has faded over its own 3 s, then the latest SIZE lands once;
    // - the flip waits for SIZE (ADR 0058, controlTick): while the tank in use crossfades its SIZE, or waits for the knob's
    //   rest to start one, the SPACE switch isn't read (ADR 0054's "finish, then go"), up to kFlipWaitsSizeCapSeconds from
    //   the switch; a SIZE moved in the flip's own 0.05 s fade-out waits for the handover.
    bool  flipFlickWaitingForTests() const { return ffWaiting_; } // read-only view for tests (SIZE waiting for the morph)
    float roomLineLengthForTests(int i) const { return room_.lineLength(i); } // read-only view for tests (samples at 24 kHz)
    // SIZE moves on only when the mean line length it means has moved by more than rel of the last one
    // (overnight review, 26 Oct 2026, arch F1; on since v1.0.37 at voicing::kSizeHysteresis; 0 = before:
    // every move). A noisy knob reading moved the lines' targets (and MASSIVE's in-loop diffusion) a
    // little every block, which restarted the FDN's rest timer (kSizeRestSeconds), so SIZE never took
    // effect in MASSIVE (and in HALL from ±2e-4 of noise).
    void  setSizeHysteresis(float rel) { sizeHyst_ = rel; }
    // PROTOTYPE, a bug fix behind a hook (overnight review, 26 Oct 2026, arch F2): the half-rate stages
    // start primed with one silent sample (dsp/HalfRate.h), so a host's first odd block mid-stream no
    // longer leaves a one-sample hole in the HALL / MASSIVE wet; the wet is then one sample late from
    // the start. Takes effect at the next reset().
    void  setHalfRatePrime(bool on) { halfPrime_ = on; }
    // The twin stops when its 50 ms level is under -100 dB, not its 1.5 s level (overnight review,
    // 26 Oct 2026, lit-plate finding 6; on since v1.0.37; false = before). That smoother falls only
    // 2.9 dB a second once the tail is gone, so the twin (a second tank) ran 26-30 s after every
    // unfreeze whatever DECAY was.
    void  setTwinStopFast(bool on) { twinStopFast_ = on; }
    // PROTOTYPES (overnight review, 26 Oct 2026; CPU options for the owner, behind renderer hooks):
    // the still lows' tank at 1/d of the rate, kLowDecimate (4, as built) .. kLowDecimateMax (8),
    // renders/still_lows_rate. Re-prepares the Engine (before the tank is laid out).
    void  setLowDecimatePrototype(int d);
    // The swell's points per side, Swell::kTaps (12, as built) .. Swell::kDraws (24, before v1.0.35),
    // for the swell and the twin's. Re-prepares the Engine.
    void  setSwellTapsPrototype(int taps);
    // The shimmer records only while ATTACK is on SHIMMER (frozen too) or its send is still fading
    // (cost-input c1): switching in, its first grains have only what was played since.
    void  setShimmerLazyPrototype(bool on) { shimmerLazy_ = on; }
    // PROTOTYPE (renders/twin_alternatives, "the layer alone"): the tank (the frozen pad) is left out of the wet,
    // so only the twin is heard; the tank still runs and freezes as in use.
    void  setLayerSoloPrototype(bool on) { layerSolo_ = on; }
    // PROTOTYPE (renders/twin_louder; owner: "even with the AS BUILT option, with wet at 100% i can hardly
    // hear the secondary reverb"): the twin's level in HALL and MASSIVE, db above today's (its output
    // only: Paint sends what it did), or no twin at all while frozen (off).
    void  setTwinGainPrototype(float db, bool off)
    {
        twinGain_ = std::pow(10.0f, db / 20.0f);
        twinOff_ = off;
    }
    // PROTOTYPE (renders/level_duck; built since v1.0.51 as voicing::kFloorDbfs -21): the floor governor's floor (dBFS).
    void  setFloorPrototype(float dbfs) { floorDbfs_ = dbfs; }
    // PROTOTYPE (renders/hall_level; owner: "Hall infinite examples feel slightly less level matched than massive ...
    // reverb feels a bit too quiet so when the set stops, there's a less seamless transition"): HALL's endless tail
    // settles `tailDb` above the floor (HALL only; as built voicing::kHallTailDb, 3 since v1.0.52, the owner's pick C),
    // and/or HALL at infinite DECAY is tilted brighter on the wet (TONE's tilt, lows / highs in dB; D, not built: 0).
    void  setHallTailPrototype(float tailDb, float tiltLowDb, float tiltHighDb)
    {
        hallTailDb_ = tailDb;
        hallTiltLowDb_ = tiltLowDb;
        hallTiltHighDb_ = tiltHighDb;
    }
    int   lowDecimate() const { return lowDecimate_; }
    int   swellTaps() const { return swell_.tapCount(); }
    // Tests only (ADR 0054, test_space_morph_waits): what the SPACE morph did, by time. Kinds: 1 a new tank starts (the old rings on),
    // 2 the two tanks swap roles, 3 a reshape in place, 4 the old tank stopped, 5 a reshape ended, 6 a freeze sent the change back
    // to the tank that had the sound.
    struct MorphEvent {
        double t;
        int    kind, from, to;
    };
    struct MorphLog {
        int        count;
        MorphEvent events[64];
    };
    const MorphLog& morphLogForTests() const { return morphLog_; }
#endif
    static constexpr float kFlipWaitsSizeCapSeconds = 1.3f; // the flip waits for SIZE: at most this from the switch (ADR 0058)
#if !defined(IV_FIXED_VOICINGS) || defined(IV_PROFILE_HOOKS) // the profile firmware's SIZEHAND lines too
    // SIZE's crossfade in groups of lines (Fdn::setStagger: 4 since v1.0.45, 2 from v1.0.39), in every FDN (the tank,
    // the still lows); 1: all lines together, as before v1.0.39; 2 or 8: the size_moves prototypes.
    void  setSizeStagger(int groups)
    {
        fdn_.setStagger(groups);
        lowFdn_.setStagger(groups);
    }
#endif
#ifndef IV_FIXED_VOICINGS
    // Tests only: false leaves TONE's resonant tilt out of the wet (the wet as it was before v1.0.44), to show the
    // bell is the only difference and the loop is untouched.
    void  setToneBellForTests(bool on) { toneBellOff_ = !on; }
    // Tests only (v1.0.77, ADR 0053): a NaN in a tank's state (0 the FDN, 1 the still lows, 2 the plate), and how many times
    // process() has restarted a tank that went invalid.
    void  poisonTankForTests(int tank);
    int   tankRestartsForTests() const { return tankRestarts_; }
#endif
    bool  twinActive() const { return twinActive_; } // read-only view for tests
    bool  unfreezeSizeWaiting() const { return ufsWaiting_; } // read-only view for tests (the room making way)
    const dsp::Transport& transport() const { return transport_; } // read-only view for tests
    bool  buttonDown() const { return buttonDown_; }
    bool  gateHigh() const { return gateHigh_; }
    // Gain the output limiter is applying now (1 = not limiting), for the LEDs.
    float limiterGain() const { return limiter_.gain(); }
    // Bytes of delay memory the Engine uses (firmware serial report).
    size_t memoryBytes() const { return (large_.used + fast_.used + axi_.used + d2_.used) * sizeof(float); }
    // False when the attached memory was too small: the Engine then passes
    // the dry only.
    bool  ready() const { return ready_; }

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }

    // Read-only views for tests.
    const dsp::Fdn&   fdn() const { return fdn_; }
    const dsp::Plate& plate() const { return plate_; }
    int   activeSpace() const { return activeSpace_; }
    float tankLevelDb() const { return activeSpace_ == 0 ? plate_.levelDb() : fdn_.levelDb(); }
    float tankSlowLevelDb() const { return activeSpace_ == 0 ? plate_.slowLevelDb() : fdn_.slowLevelDb(); }

private:
    static constexpr int kChunk = dsp::Fdn::kControlBlock;
    // The resting twin room's rings zeroed this many floats a chunk (TwinRoom::clearSome; 1 KB, ~41 chunks for all).
    static constexpr int kRoomClearSlice = 256;
    static constexpr int kLowDecimate = 4; // still lows' low tank runs at 12 kHz
#ifndef IV_FIXED_VOICINGS
    static constexpr int kLowDecimateMax = 8; // setLowDecimatePrototype; the chunk buffers below fit any d >= 4
    int lowDecimate_ IV_RI(kLowDecimate);
#else
    static constexpr int lowDecimate_ = kLowDecimate;
#endif
    static constexpr int kMaxEdges = 16;

    void queueEdge(bool isButton, bool level, int offset);
    void handleEdges(int chunkStart, int chunkEnd);
    void onGesture(GestureReader::Event e);
    void controlTick(int n);
    void predelayCrossfade(float l, float r, float& pl, float& pr);
    // snap: jump to the targets (reset, a SPACE flip); snapInput: the input diffusers' amount and
    // the shimmer's recording too (not on a flip, where they keep running and glide).
    void applySpace(bool snap, int n = kChunk, bool snapInput = true);
    void setEarlyTapsForSpace();
    float decayRate(float decay, float size) const;
    void  resetHalf(dsp::HalfRate<kChunk>& h) const
    {
        h.reset();
#ifndef IV_FIXED_VOICINGS
        if (halfPrime_) h.prime(); // setHalfRatePrime
#endif
    }
    float hysteresisSize(float size, float& held, bool snap) const; // SIZE's hysteresis (v1.0.37)
#ifndef IV_FIXED_VOICINGS
#endif
    float sizeHyst_ = voicing::kSizeHysteresis; // setSizeHysteresis
    float sizeHeld_ = -1.0f, twinSizeHeld_ = -1.0f; // the SIZE the tank and the twin have (< 0: none yet)
    // ERA last sent to a tank (applySpace): true, keeping m, when m differs from it.
    enum { EraSentFdn, EraSentPlate, EraSentTwinPlate, EraSentCount };
    bool eraChanged(int which, const dsp::EraMix& m);
    float smoothed(ParamId id) const { return smooth_[static_cast<size_t>(id)]; }

    float sampleRate_ IV_RI(48000.0f);
    int   maxBlockSize_ IV_RI(48);
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smooth_{};
    bool  buttonDown_ IV_ZI, gateHigh_ IV_ZI;
    bool  ready_ IV_ZI;

    dsp::Arena large_, fast_;
#if !defined(IV_NO_HEAP)
    std::vector<float> ownLarge_, ownFast_, ownAxi_, ownD2_;
#endif
    float* attachedLarge_ IV_ZI;
    float* attachedFast_ IV_ZI;
    size_t attachedLargeCount_ IV_ZI, attachedFastCount_ IV_ZI;

    dsp::DelayLine    predelay_[2];
    float             predelayNow_ IV_ZI, predelayTarget_ IV_ZI; // samples
    dsp::Diffuser<4>  diffuser_[2];
    dsp::Fdn          fdn_;
    dsp::Plate        plate_;
    // Still lows (v1.0.18, ADR 0012 amendment; docs/research/still-lows.md): the HALL / MASSIVE tank's
    // input splits at kStillLowsHz (Butterworth); the highs go to fdn_, the lows to lowFdn_, an
    // unmodulated tank at 1/kLowDecimate the rate that follows fdn_'s SIZE, DECAY and governor
    // (the established two-tank design: Keith Barr's dual-band reverb, Eventide's H90 Hall).
    void              processTwoTanks(int n);
    dsp::Fdn          lowFdn_;
    bool              lowTank_ = true;
    static constexpr float kLowAntiAliasHz = 2000.0f;
    float             xoL_[5] IV_ZI, xoH_[5] IV_ZI, aaL_[5] IV_ZI; // biquads b0 b1 b2 a1 a2: crossover LP/HP, 2 kHz LP
    float             xoZ_[2][6][2] IV_ZI;              // per channel: in LP, in AA, in HP, (spare), out AA
    int               lowPhase_ IV_ZI;
    float             lowPrev_[2] IV_ZI, lowCur_[2] IV_ZI;
    float             lowInL_[kChunk / kLowDecimate + 2] IV_ZI, lowInR_[kChunk / kLowDecimate + 2] IV_ZI;
    float             lowOutL_[kChunk / kLowDecimate + 2] IV_ZI, lowOutR_[kChunk / kLowDecimate + 2] IV_ZI;
    dsp::HalfRate<kChunk> tankHalf_, twinHalf_; // fdn_ and the twin's room run at half the rate (v1.0.35, ADR 0026)
#ifndef IV_FIXED_VOICINGS
    int               fitEarlyTaps_ IV_ZI; // setEarlyTapPrototype; 0 = as built
    uint32_t          fitSwellSeed_ IV_ZI; // setSwellSeedPrototype; 0 = as built
    bool              halfPrime_ IV_ZI; // setHalfRatePrime; false = as built
    bool              twinStopFast_ IV_RI(true); // setTwinStopFast; false = before v1.0.37
    int               fitSwellTaps_ IV_ZI; // setSwellTapsPrototype; 0 = as built
    bool              layerSolo_ IV_RI(false); // setLayerSoloPrototype
    float             twinGain_ IV_RI(1.0f);  // setTwinGainPrototype; 1 = as built
    bool              twinOff_ IV_RI(false);
    bool              shimmerLazy_ IV_ZI, shimmerArmed_ IV_RI(true), shimmerResting_ IV_ZI; // setShimmerLazyPrototype
    float             floorDbfs_ IV_RI(voicing::kFloorDbfs);                     // setFloorPrototype
    float             hallTailDb_ IV_RI(voicing::kHallTailDb);                  // setHallTailPrototype
    float             hallTiltLowDb_ IV_ZI, hallTiltHighDb_ IV_ZI;          // setHallTailPrototype (not built)
    float floorDbfs() const { return floorDbfs_; }
    float hallTailDb() const { return hallTailDb_; }
#else
    static constexpr float floorDbfs() { return voicing::kFloorDbfs; }
    static constexpr float hallTailDb() { return voicing::kHallTailDb; }
#endif
    float             fwsWait_ IV_ZI, fwsSize_ IV_ZI; // the flip waits for SIZE: how long the switch has waited; the SIZE the tank keeps in a fade-out
    bool  tankSizeBusy() const;           // the tank in use crossfades its SIZE, or waits to
    dsp::Plate        twinPlate_; // PLATE's twin
    // HALL's and MASSIVE's twin (v1.0.43, the owner's pick E on renders/small_room): the small room, its rings in
    // AXI SRAM; its gain by SPACE (HALL, MASSIVE: ~8 dB under a frozen pad); its 50 ms power (the twin stops on
    // it) and that one-pole's step for a block of roomStepN_; twinSized_: SIZE has reached it since it started.
    dsp::TwinRoom     room_;
    float             roomGain_[2] = {1.0f, 1.0f}, roomLevel_ IV_ZI, roomStep_ IV_ZI;
    float             roomTrim_ = 1.0f; // the room's output trim over SIZE (v1.0.47, voicing::kTwinRoomTrimDb)
    float             roomSize_ IV_ZI; // the SIZE the room keeps (ADR 0056): heldSize_ when it started
    int               roomStepN_ IV_RI(-1);
    bool              twinSized_ IV_ZI;
    dsp::Diffuser<4>  twinDiffuser_[2];
    float             twinIn_ IV_ZI, twinInStep_ IV_ZI;
    bool              twinActive_ IV_ZI;
    // The room makes way (applySpace): SIZE waiting for it; its make-way gain (1 .. 0) and that gain's step a sample (0 resting,
    // < 0 fading out, > 0 back up after a new freeze).
    bool              ufsWaiting_ IV_ZI;
    float             ufsGain_ IV_RI(1.0f), ufsStep_ IV_ZI;
    dsp::Shimmer      shimmer_; // ATTACK's third position (ADR 0024)
    float             eraMakeup_ IV_RI(1.0f);
    // The limiter (SPEC §4.10).
    dsp::Tilt         tilt_;
    dsp::ToneBell     toneBell_; // TONE's resonant tilt, after tilt_ (v1.0.44)
    // MOTION's left side: the tape transport on the wet (v1.0.46, dsp/Transport.h), and what it was last told.
    dsp::Transport         transport_;
    dsp::TransportAmounts  transportAmt_;
    bool                   transportOn_ IV_ZI;
#ifndef IV_FIXED_VOICINGS
    bool              toneBellOff_ IV_ZI; // setToneBellForTests
    int               tankRestarts_ IV_ZI; // tankRestartsForTests
#endif
    dsp::Limiter      limiter_;
    // ERA's sources into the tank's input (SPEC §4.7): the tape's hiss, the Midiverb's crackle.
    uint32_t          noiseSeed_ IV_RI(0xA11CEu);
    float             hissLp_[2] IV_ZI, hissGain_ IV_ZI, crackleChance_ IV_ZI, crackleGain_ IV_ZI;
    float             lastMean_ IV_RI(-1.0f), lastSpread_ IV_RI(-1.0f);
    float             hallSize_ IV_RI(0.5f); // HALL's SIZE for its gains, glided (applySpace)

    // SPACE changes (ADR 0013, 0054): the tank's input fades out and back, and at the silent point between (morphFlip) the morph or
    // the reshape starts; the tail keeps ringing through it.
    int   activeSpace_ IV_RI(1), flipPhase_ IV_ZI; // 0 steady, 1 fading out, 2 fading in
    float flipGain_ IV_RI(1.0f), flipStep_ IV_ZI;
    // An ONSET change dips the swell out and back in (applySpace): the ONSET it is voiced for, the dip
    // (0 none .. 1 out) and its step per sample.
    int   swellOnset_ IV_ZI;
    float onsetDip_ IV_ZI, onsetStep_ IV_ZI;
    // ATTACK = SHIMMER: the octave's send fades in and out over kAttackFadeSeconds; it rests while
    // frozen. shimmerAmount_: TIME, how much shimmer.
    float shimmerMix_ IV_ZI, shimmerTarget_ IV_ZI, shimmerAmount_ IV_ZI;
    float modScale_ = 1.0f;  // test hook (setModulationScale)
    // TIME's pre-delay crossfade (Engine::controlTick, ADR 0025): the old delay, the rest before it
    // starts, the fade's gains (stepped by a rotation).
    float pdOld_ IV_ZI, pdLastTarget_ IV_ZI, pdRest_ IV_ZI, pdOldG_ IV_ZI, pdNewG_ IV_RI(1.0f), pdRotC_ = 1.0f, pdRotS_ IV_ZI;
    int   pdXfLeft_ IV_ZI;
#if !defined(IV_MODE_PROFILE)
    float wetLift_ IV_RI(1.0f), wetLiftNow_ IV_RI(1.0f); // the wet's lift at short tails, after WEAR (v1.0.73, ADR 0049)
#endif
    float decayMakeup_ = 1.0f, decayGain_ IV_RI(1.0f), decayGainNow_ IV_RI(1.0f); // DECAY's level make-up (v1.0.28)
    float holdMix_ IV_ZI; // endless (v1.0.61): how far its loss has glided out after the input went quiet
    float holdFloor_ IV_ZI; // and how far the finite top's floor has let go
    dsp::EraTexture texture_; // ERA's wear on the wet (v1.0.42)
    bool texOn_ IV_ZI;
    float diffMotionNow_ IV_ZI, diffMotionPhase_ IV_ZI; // the PLATE's wandering input allpass, samples
    float hallInLoop_ = voicing::kHallInLoopDiffusion; // setHallInLoopDiffusion overrides it (renders/hall_diffusion)

    // Freeze (ADR 0009–0011). Button and gate edges this block, by offset.
    struct Edge {
        int  offset;
        bool isButton, level;
    };
    Edge          edges_[kMaxEdges] IV_ZI;
    int           edgeCount_ IV_ZI;
    GestureReader gesture_;
    uint64_t      clock_ IV_ZI;            // samples since prepare
    float heldSize_ = 0.5f, heldDecay_ = 0.5f; // the big tank's SIZE and DECAY, frozen with it
    float freezeMix_ IV_ZI;             // 0 normal .. 1 a still pad (in-loop colour out, gain 1)
    float tankIn_ IV_RI(1.0f), tankInStep_ IV_ZI;
    float paint_ IV_ZI, paintRoom_ IV_RI(1.0f); // paintRoom_: 1 → 0 as a painted pad nears its ceiling
    float padFloorDb_ = -24.0f;
    bool  padCaptured_ IV_ZI; // the pad's own level has been taken as its ceiling
    uint64_t frozeAt_ IV_ZI;
    // Input quiet detector for holding an infinite tail where it settled.
    float    inputPower_ IV_ZI;
    float    tankInPower_ IV_ZI; // the big tank's input (after the swell), for the quiet detector
    uint64_t quietSince_ IV_ZI;
    uint64_t endlessPlayedAt_ IV_ZI; // endless (v1.0.61): last block its input sustained the tail
    bool     tailCaptured_ IV_ZI;
    float    tailCeilingDb_ = -24.0f;
    float tankInTarget_ IV_RI(1.0f);

    // MIX, ramped per sample across each control chunk.
    float dryNow_ IV_RI(1.0f), wetNow_ IV_ZI, dryStep_ IV_ZI, wetStep_ IV_ZI;

    // Scratch for one chunk.
    float bufL_[kChunk] IV_ZI, bufR_[kChunk] IV_ZI, wetL_[kChunk] IV_ZI, wetR_[kChunk] IV_ZI;
    float twinInL_[kChunk] IV_ZI, twinInR_[kChunk] IV_ZI, twinL_[kChunk] IV_ZI, twinR_[kChunk] IV_ZI;
    float chunkGain_[kChunk] IV_ZI; // the tank's input gain (tankIn_ x flipGain_) per sample, kept for after the swell
    static_assert(kChunk <= dsp::Swell::kMaxBlock, "the swell runs a chunk at a time");
    // The ERA each tank was sent last (eraChanged), as an EraMix's bytes; eraSentOk_: which are set.
    unsigned char eraSent_[EraSentCount][sizeof(dsp::EraMix)];
    uint8_t       eraSentOk_ IV_ZI;

    // The swells, near the end (they followed the input diffusers): the members after them load at
    // shorter offsets, and the swell's TIME crossfade state moves only what follows (flash: in their old
    // place that state cost ~470 B more, in the Engine's load offsets).
    dsp::Swell        swell_;
    dsp::Swell        twinSwell_; // PLATE's twin's (with no attack); HALL's and MASSIVE's room has none (v1.0.43)

    // The allpass lines' memory (attachMemory's axi): last, so no other member's offset moves (the
    // per-sample code's load offsets, and with them its flash, stay as they were).
    dsp::Arena axi_;
    float*     attachedAxi_ IV_ZI;
    size_t     attachedAxiCount_ IV_ZI;
    dsp::Arena d2_; // attachMemory's d2 (likewise last)
    float*     attachedD2_ IV_ZI;
    size_t     attachedD2Count_ IV_ZI;

    // Control maths kept between blocks (CPU save 2, 28 Oct 2026; last, so no other member's offset moves): blockK()'s
    // recent coefficients; the knobs' smoothing coefficients for two chunk lengths (controlTick).
    float blockK(float seconds, int n);
    void  trackBrightness(const float* inL, const float* inR, int n); // a held endless tail's settle (v1.0.58)
    struct BlockMemo {
        float seconds;
        int   n; // 0: empty
        float k;
    };
    static constexpr int kBlockMemo = 16;
    BlockMemo blockMemo_[kBlockMemo] IV_ZI;
    int       blockNext_ IV_ZI;
    float     smoothK_[2][static_cast<size_t>(ParamId::Count)] IV_ZI;
    int       smoothN_[2] IV_ZI, smoothNext_ IV_ZI;
    dsp::EarlyLayouts earlyLayouts_; // the tanks' early-tap layouts (Fdn::layoutEarly)
    // HALL's floor scale over kHallFloorScale (v1.0.57: 1 up to a 5 s tail, 2 from 15 s), glided; set on every
    // way into HALL (applySpace's snap), so it needs no initialiser. Last: no other member's offset moves.
    float hallFloorNow_ IV_ZI;
    float hallFullerNow_ IV_ZI; // v1.0.71: x2 up to DECAY's crossover, x1 at endless, glided; set as hallFloorNow_ is
    // A held endless tail (v1.0.58, applySpace): the share of its top end's mellow still to come (1 on every block
    // until the tail is held, so it needs no initialiser), and how far its modulation has calmed (0..1).
    float endlessTopLeft_ IV_ZI, endlessCalm_ IV_ZI;
    // The playing's brightness (v1.0.58, voicing::kEndlessBright*): its slope's power and its power, followed while
    // the input is above the quiet level (process()), so a held tail keeps what was played into it.
    float brightSlope_ IV_ZI, brightPower_ IV_ZI;
    // The wet's input without its DC (v1.0.58, voicing::kInputDcHz): dsp::DcBlocker's one pole (prepare: its
    // coefficients), without its constructor (flash).
    float inDcR_ IV_ZI, inDcB_ IV_ZI, inDcX_[2] IV_ZI, inDcY_[2] IV_ZI;
    float inputDc(int c, float x)
    {
        const float y = inDcB_ * (x - inDcX_[c]) + inDcR_ * inDcY_[c];
        inDcX_[c] = x;
        inDcY_[c] = y;
        return y;
    }
    // The PLATE's SIZE crossfade state (dsp::PlateXf, v1.0.68; the twin glides). Last: no other member's offset moves.
    dsp::PlateXf plateXf_;
    // The SPACE morph (ADR 0054). Last: no other member's offset moves.
#ifndef IV_FIXED_VOICINGS
    void  morphNote(int kind, int from, int to);
    MorphLog morphLog_{};
#endif
    void  morphFlip(int to, int n);          // the flip's silent point
    void  morphAdvance(int n);               // once a chunk: the fades and the in-place ramps
    void  morphStopTwin();                   // the twin of the old space stops at the flip's silent point
    void  morphHoldOld();                    // a freeze early in a morph: the old tank stays, the change waits
    void  startTwin();                       // the twin starts: at a freeze, or a reshape's end
    float ufsSize(float size, bool snap);    // the room makes way: the SIZE the tank takes after an unfreeze (applySpace)
    void  ufsRamp(int n);                    // the room making way: its gain over this chunk; it stops at 0 (process)
    float ffHold(float size, bool snap);     // SIZE waits for the morph: the SIZE the new tank takes during a two-tank morph
    static constexpr float kMorphDropGain = 0.05f; // morphHoldOld: under this the new tank's cut can't be heard (click check)
    bool  morphKeepTank_ IV_ZI;
    uint8_t morphLightWait_ IV_ZI;           // control ticks left before the heads may rest after a change (morphAdvance)
    void  morphLend(float* ol, float* orr);
    void  morphRunOld(int n);                // the old tank over this chunk, mixed into wetL_ / wetR_
    float tankFastDb(int tank) const { return tank == 0 ? plate_.fastLevelDb() : fdn_.fastLevelDb(); }
    // All set before they're read: morphOld_ by reset() (via prepare()), the rest by morphFlip's fresh start.
    int   morphOld_ IV_RI(-1);            // the tank ringing out: -1 none, 0 the plate, 1 fdn_ and lowFdn_
    int   morphOldSpace_ IV_ZI;           // the SPACE it was voiced for
    float morphX_ IV_ZI;                  // how far the morph is, 0..1
    float morphGOld_ IV_ZI, morphGNew_ IV_ZI, morphGOldWas_ IV_ZI, morphGNewWas_ IV_ZI; // per chunk, ramped per sample
    float morphOldDecayGain_ IV_ZI, morphOldLift_ IV_ZI; // the old tank's own make-ups, held
    bool  morphFast_ IV_ZI;               // a freeze: the old tank goes in 0.3 s and stops hearing the music
    bool  morphFreezeSeen_ IV_ZI;
    float morphInL_[kChunk] IV_ZI, morphInR_[kChunk] IV_ZI;
    enum { kPlaceRate, kPlaceHigh, kPlaceGovernor, kPlaceFloorDb, kPlaceDecayGain, kPlaceWetLift, kPlaceFloorScale,
           kPlaceFloorCentre, kPlaceAir, kPlaceEndlessTop, kPlaceShelf, kPlaceLowsOwn, kPlaceCount };
    float placeRamp(int id, float target);
    // As above: placeOn_ guards them, and morphFlip sets them.
    bool  placeOn_ IV_ZI;
    float placeT_ IV_ZI, placeS_ IV_ZI, placeSeconds_ IV_ZI, earlyW_ IV_ZI;
    float placeNow_[kPlaceCount] IV_ZI, placeStart_[kPlaceCount] IV_ZI;
    float lowsOwnNow_ IV_ZI;              // applySpace, whenever a tank is voiced
    // SIZE waits for the morph (ffHold): a SIZE waiting for the old tank to go; the SIZE the new tank has (set at the flip's
    // snap, or by a freeze).
    bool  ffWaiting_ IV_ZI;
    float ffSize_ IV_ZI;
};

} // namespace iv
