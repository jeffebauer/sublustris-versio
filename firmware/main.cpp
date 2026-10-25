// Firmware Host (SPEC §6, §7 M0/M3). One file, three variants selected at
// build time by the Makefile's MODE (see firmware/Makefile and
// firmware/README.md). Seeded from Resilio Versio's firmware (ADR 0001):
// the variants, the flash measures (no_uart_spi.cpp) and the LED PWM come
// from there; everything Resilio-specific is gone.
//
//   IV_MODE_M0TEST  - SPEC §7 M0 test build. Dry passthrough, controls -> LEDs
//                      + serial, for the hardware check. Doesn't link the Core.
//   IV_MODE_PROFILE - SPEC §7 M3 hardware profiling. Ignores knobs/switches
//                      (no rack power expected), cycles a fixed corner table,
//                      feeds the Engine a synthetic test signal, reports
//                      CpuLoadMeter stats per corner over serial.
//   IV_MODE_RELEASE - The real instrument (default). Knobs/switches/button/
//                      gate -> ParamSpec -> Engine, LEDs as level meters plus
//                      the freeze tint (SPEC §3). No serial logging (flash).
//
// Each variant's code lives in its own #if block below; they share nothing
// but BootPattern(), the pot table and the include list, so any one variant
// can be read top-to-bottom without cross-referencing another.
//
// Memory placement (SPEC §5.2): DTCM (zero-wait, 128 KB) is for the hottest
// small buffers (input diffusers, the twin's diffusers); IV_DTCM below places a
// buffer there. .dtcmram_bss is NOLOAD (startup doesn't zero it), so whoever
// uses it clears it first. The long tank lines go to SDRAM (M1). The M0
// Engine is a passthrough and has no buffers yet.

#include "daisy_versio.h"
#if !defined(IV_MODE_M0TEST)
#include "dsp/Engine.h"
#include "dsp/EraStages.h"
#include "dsp/Tape.h"
#include "params/Mappings.h"
#include "params/ParamSpec.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// Tightly-coupled data RAM (see "Memory placement" above).
#define IV_DTCM __attribute__((section(".dtcmram_bss")))

using namespace daisy;

#if !defined(IV_MODE_M0TEST) && !defined(IV_MODE_PROFILE) && !defined(IV_MODE_RELEASE)
#define IV_MODE_RELEASE 1
#endif

#ifndef IV_PROFILE_BLOCK
#define IV_PROFILE_BLOCK 48
#endif

namespace {

DaisyVersio hw;

// The SDRAM as write-back WITH write-allocate (ST AN4839, "Level 1 cache on STM32F7 and STM32H7":
// the attribute for external memory that is read and written a lot). libDaisy's System::ConfigureMpu
// makes it write-back without write-allocate (TEX 0, C, B), so every write to a line not in the cache
// goes out to the SDRAM on its own: the tanks write ~35 scattered samples a sample, and the tank's
// reads queued behind them (M3 round 4: its output and early taps, ~30 reads, cost ~4,500 cycles a
// sample). With write-allocate a line's 8 writes merge in the cache and go out as one burst. Nothing
// DMAs to or from the SDRAM, so no coherency changes. Called right after hw.Init(), before any SDRAM use.
void SdramWriteAllocate()
{
    SCB_CleanDCache();
    HAL_MPU_Disable();
    MPU_Region_InitTypeDef r{};
    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = MPU_REGION_NUMBER1; // libDaisy's SDRAM region, redefined
    r.BaseAddress      = 0xC0000000;
    r.Size             = MPU_REGION_SIZE_64MB;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.TypeExtField     = MPU_TEX_LEVEL1; // TEX 1 + C + B: write-back, read and write allocate
    r.IsCacheable      = MPU_ACCESS_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_BUFFERABLE;
    r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    r.SubRegionDisable = 0x00;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&r);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    __DSB();
    __ISB();
}
#if !defined(IV_MODE_M0TEST)
// The Engine is built at boot (placement new at the top of main), not as a
// plain global: an object with default member values is .data, stored in
// flash and copied to RAM at startup, while zeroed .bss storage costs no
// flash (128 KB budget, SPEC §5.3; Resilio measured ~7.9 KB for its Tank).
// Never destroyed (main never returns).
// In DTCM (overnight review 26 Oct 2026, lit-m7 section 4.3): the Engine's own state (~24 KB, read
// and written every sample) sat in cached AXI SRAM and fought the delay lines for the 16 KB data
// cache; DTCM is zero-wait and never cached. .dtcmram_bss is NOLOAD, but `iv::Engine()` (value
// initialisation) zeroes the whole object before constructing it (a memset in main), so nothing
// depends on startup's .bss clear. DTCM budget (release map): gFast 36,864 + this 24,208 = 61,072 B
// from 0x20000000; the stack grows down from _estack = 0x20020000, leaving ~68 KB for it.
IV_DTCM alignas(iv::Engine) unsigned char gEngineStorage[sizeof(iv::Engine)];
iv::Engine& engine = *reinterpret_cast<iv::Engine*>(gEngineStorage);

// Delay memory (SPEC §5.2): the long lines (FDN, predelay) in SDRAM, the
// small hot buffers (input diffusers) in DTCM. Both sections are NOLOAD, so
// startup doesn't zero them; the Engine's Arena zeroes what it takes. Sized
// for 48 kHz: at any other rate the Engine finds too little memory and
// passes the dry only (Engine::ready()).
constexpr size_t kLargeFloats = iv::Engine::largeFloats(48000.0f);
constexpr size_t kFastFloats  = iv::Engine::fastFloats(48000.0f);
DSY_SDRAM_BSS float gLarge[kLargeFloats];
IV_DTCM float       gFast[kFastFloats];
// The tank's and the low tank's in-loop allpass lines (Engine::axiFloats, ~424 KB at 48 kHz) in AXI
// SRAM: a plain global, so libDaisy's .bss (the 512 KB SRAM at 0x24000000), zeroed by startup.
// Overnight review, 26 Oct 2026: 32 of the tanks' ~80 SDRAM streams a tank sample come on chip.
constexpr size_t kAxiFloats = iv::Engine::axiFloats(48000.0f);
alignas(32) float gAxi[kAxiFloats];
#endif

// Panel pots P1..P7 (reading order: top to bottom, left to right) -> libDaisy
// knob index. A hardware fact from Resilio's M0 LED check (29 Sep 2026):
// libDaisy's KNOB_0..KNOB_6 are NOT in reading order. Each pot's CV input is
// summed with it in hardware, so CV follows its pot.
constexpr int kPotKnob[DaisyVersio::KNOB_LAST] = {0, 4, 2, 1, 5, 3, 6};

// Boot pattern shared by all three variants (NE convention: a unique colour
// sequence on power-up confirms this firmware loaded, distinct from stock).
// Immensio: cool blue -> violet -> white, so it can't be mistaken for Resilio.
void BootPattern()
{
    const float colours[4][3] = {{0, 0.3f, 1}, {0.5f, 0, 1}, {0, 0, 1}, {1, 1, 1}};
    for (int step = 0; step < 4; ++step) {
        for (int led = 0; led < 4; ++led) {
            const float* c = colours[(led + step) % 4];
            hw.SetLed(size_t(led), c[0], c[1], c[2]);
        }
        hw.UpdateLeds();
        System::Delay(150);
    }
}

#if !defined(IV_MODE_PROFILE) // the profile build reads no controls
// libDaisy Switch3 position -> left / centre / right = 0 / 1 / 2.
int SwitchPosition(int sw, bool invert)
{
    int pos;
    switch (hw.sw[sw].Read()) {
        case Switch3::POS_LEFT: pos = 0; break;
        case Switch3::POS_RIGHT: pos = 2; break;
        default: pos = 1;
    }
    return invert ? (2 - pos) : pos;
}
#endif

} // namespace

// =============================================================================
#if defined(IV_MODE_M0TEST)
// =============================================================================
// Firmware Host, M0 test build (SPEC §7 M0, docs/m0-hardware-check.md).
// - Audio: dry passthrough, input copied straight to output (no trim, so the
//   check measures the module's own analog path).
// - Controls -> LEDs, in panel order, so every control can be checked
//   without a computer:
//     LED_0 R/G/B = P1 MIX,  P2 DECAY, P3 TONE
//     LED_1 R/G/B = P4 ERA,  P5 SIZE,  P6 DRIFT
//     LED_2 R = P7 TIME, G = SW0 SPACE position, B = SW1 ATTACK position
//     LED_3 white while button held, red flash on each gate rising edge,
//           otherwise green when every knob reads <= 0.02 or >= 0.98
//           (checks 0 V / 5 V CV without a computer: rack power and USB
//           must not be connected at the same time)
// - Controls -> USB serial, 10 times a second, exact values (x1000), pots
//   in panel order P1..P7.

namespace {

constexpr int kBlockSize = 48;

volatile int      gateEdges      = 0;
volatile int      buttonPresses  = 0;
volatile uint32_t gateFlashUntil = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    hw.tap.Debounce(); // ProcessAllControls() only handles knobs

    static bool lastGate = false;
    const bool gate      = hw.Gate();
    if (gate && !lastGate) {
        ++gateEdges;
        gateFlashUntil = System::GetNow() + 60;
    }
    lastGate = gate;
    if (hw.tap.RisingEdge()) {
        ++buttonPresses;
    }

    for (size_t i = 0; i < size; ++i) {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int Milli(float v) { return int(v * 1000.0f + 0.5f); }

} // namespace

int main()
{
    hw.Init(true); // boost to 480 MHz
    SdramWriteAllocate();
    hw.SetAudioBlockSize(kBlockSize);
    BootPattern();

    hw.seed.StartLog(false); // don't block waiting for a serial monitor
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    uint32_t nextPrint = 0;
    while (true) {
        float p[DaisyVersio::KNOB_LAST]; // P1..P7
        for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i)
            p[i] = hw.GetKnobValue(kPotKnob[i]);
        const int space  = SwitchPosition(DaisyVersio::SW_0, false);
        const int attack = SwitchPosition(DaisyVersio::SW_1, false);

        hw.SetLed(DaisyVersio::LED_0, p[0], p[1], p[2]);
        hw.SetLed(DaisyVersio::LED_1, p[3], p[4], p[5]);
        hw.SetLed(DaisyVersio::LED_2, p[6], space * 0.5f, attack * 0.5f);
        if (hw.tap.Pressed())
            hw.SetLed(DaisyVersio::LED_3, 1, 1, 1);
        else if (System::GetNow() < gateFlashUntil)
            hw.SetLed(DaisyVersio::LED_3, 1, 0, 0);
        else {
            bool allAtExtremes = true;
            for (float v : p)
                allAtExtremes &= (v <= 0.02f || v >= 0.98f);
            hw.SetLed(DaisyVersio::LED_3, 0, allAtExtremes ? 1.0f : 0.0f, 0);
        }
        hw.UpdateLeds();

        const uint32_t now = System::GetNow();
        if (now >= nextPrint) {
            nextPrint = now + 100;
            hw.seed.PrintLine("P %4d %4d %4d %4d %4d %4d %4d | SW %d %d | BTN %d %d | GATE %d %d",
                              Milli(p[0]), Milli(p[1]), Milli(p[2]), Milli(p[3]), Milli(p[4]), Milli(p[5]), Milli(p[6]),
                              space, attack, int(hw.tap.Pressed()), buttonPresses, int(hw.Gate()), gateEdges);
        }
        System::Delay(1);
    }
}

// =============================================================================
#elif defined(IV_MODE_PROFILE)
// =============================================================================
// Hardware profiling build (SPEC §7 M3). Runs on USB power alone (no rack
// power, so no real audio in and knob/CV readings would be floating/garbage):
// knobs and switches are ignored entirely. Instead the firmware cycles
// through a fixed table of setting "corners" (SPACE x DECAY x SIZE x ERA,
// SPEC §5.1's worst case included), feeding the Engine a synthetic click +
// low-level noise signal so the loops stay busy (denormal paths exercised,
// not just decaying to silence between corners). CpuLoadMeter reports
// per-corner average/max/min load over serial, and dsp/ProfileHook.h's
// sections split it.
//
// ATTACK is BLOOM and MIX, TONE, DRIFT and TIME are at their top on every
// corner (SPEC §5.1: the swell stage runs, the reads move furthest). The M0
// Engine is a passthrough, so until M1 these numbers measure the callback's
// own overhead only.

#include "util/CpuLoadMeter.h"
#include "dsp/ProfileHook.h"

namespace {

using iv::ParamId;

constexpr int kBlockSize = IV_PROFILE_BLOCK;

// ---- Hand-rolled serial line formatting (flash budget, SPEC §5.3) ---------
// No printf/vsnprintf/std::snprintf in this build: the nano-newlib printf
// family costs ~2.1 KB of flash once pulled in (Resilio measured), all for
// one line every ~3 s. This firmware links --specs=nano.specs without
// `-u _printf_float` anyway, so float formats wouldn't print. Integer maths
// here; output goes straight to the USB CDC transport (hw.seed.usb_handle).
void AppendStr(char*& p, const char* end, const char* s)
{
    while (*s && p < end) *p++ = *s++;
}

void AppendPadRight(char*& p, const char* end, const char* s, int width)
{
    int n = 0;
    while (*s && p < end) {
        *p++ = *s++;
        ++n;
    }
    while (n < width && p < end) {
        *p++ = ' ';
        ++n;
    }
}

void AppendUInt(char*& p, const char* end, unsigned v)
{
    char digits[10];
    int  n = 0;
    do {
        digits[n++] = char('0' + v % 10);
        v /= 10;
    } while (v && n < int(sizeof(digits)));
    while (n > 0 && p < end) *p++ = digits[--n];
}

void AppendInt(char*& p, const char* end, int v)
{
    if (v < 0 && p < end) *p++ = '-';
    AppendUInt(p, end, unsigned(v < 0 ? -v : v));
}

// Right-justifies "value.d" (one decimal digit) into `width` characters,
// e.g. AppendFixed1(p, end, 583, 5) -> " 58.3" (value is the number x10).
void AppendFixed1(char*& p, const char* end, int tenths, int width)
{
    if (tenths < 0) tenths = 0;
    char digits[8];
    int  n      = 0;
    int  whole  = tenths / 10;
    int  frac   = tenths % 10;
    digits[n++] = char('0' + frac);
    digits[n++] = '.';
    do {
        digits[n++] = char('0' + whole % 10);
        whole /= 10;
    } while (whole && n < int(sizeof(digits)));
    int pad = width - n;
    while (pad-- > 0 && p < end) *p++ = ' ';
    while (n > 0 && p < end) *p++ = digits[--n];
}

// Fraction (0..1) -> tenths of a percent, e.g. 0.583f -> 583 ("58.3").
int FracToPercentTenths(float frac) { return int(frac * 1000.0f + 0.5f); }

constexpr size_t kLineBufSize = 1100; // CORNER + SPLIT + PEAK (+ BENCH) lines, sent in one transmit

void TransmitLine(const char* buf, size_t len)
{
    hw.seed.usb_handle.TransmitInternal(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buf)), len);
}

// ---- Corner table (SPEC §5, §7 M3) -----------------------------------------
// SPACE PLATE/HALL/MASSIVE x DECAY {0,1} x SIZE {0,1} x ERA {0,1} = 24
// corners, all fully wet (MIX 1) so the meter sees the Engine, not the dry
// mix. SPEC §5.1's worst case (MASSIVE, Era full, BLOOM) is flagged
// "(worst?)" in the printed name; freeze + paint is added at M5.
struct Corner {
    int   spacePos; // 0/1/2 -> PLATE/HALL/MASSIVE (Switch3 encoding)
    float decay, size, era;
    char  name[40];
};

// M3 round 10: every era (the centre of each one's stretch: Neve, Midiverb, bucket-brigade, cassette,
// AM radio), SIZE 0 / 1, DECAY full.
constexpr float kCornerEras[5] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
const char* const kEraNames[5] = {"neve", "midiverb", "bbd", "cassette", "radio"};
constexpr int kNumCorners = 3 * 2 * 5;
Corner        gCorners[kNumCorners];

const char* const kSpaceNames[3] = {"PLATE", "HALL", "MASSIVE"};

void BuildCornerTable()
{
    int idx = 0;
    for (int spacePos = 0; spacePos < 3; ++spacePos) {
        for (int size = 0; size < 2; ++size) {
            for (int era = 0; era < 5; ++era) {
                Corner& c  = gCorners[idx++];
                c.spacePos = spacePos;
                c.decay    = 1.0f;
                c.size     = float(size);
                c.era      = kCornerEras[era];
                char*       p   = c.name;
                const char* end = c.name + sizeof(c.name) - 1;
                AppendStr(p, end, kSpaceNames[spacePos]);
                AppendStr(p, end, " S");
                AppendInt(p, end, size);
                AppendStr(p, end, " ");
                AppendStr(p, end, kEraNames[era]);
                *p = '\0';
            }
        }
    }
}

void ApplyCorner(int idx)
{
    const Corner& c = gCorners[idx];
    engine.setParam(ParamId::Space, iv::switchToNormalised(c.spacePos));
    engine.setParam(ParamId::Decay, c.decay);
    engine.setParam(ParamId::Size, c.size);
    engine.setParam(ParamId::Era, c.era);
    engine.setParam(ParamId::Mix, 1.0f);
    engine.setParam(ParamId::Tone, 1.0f);
    engine.setParam(ParamId::Drift, 1.0f);
    engine.setParam(ParamId::Time, 1.0f);
    engine.setParam(ParamId::Attack, iv::switchToNormalised(1)); // BLOOM
}

// ---- Synthetic test signal: click every 500 ms + low-level seeded noise ---
constexpr float kCornerSeconds      = 3.0f;
size_t          gCornerSamples      = 0; // set once fs is known, in main()
size_t          gClickPeriodSamples = 0;
size_t          gClickPhase         = 0;
uint32_t        gNoiseState         = 0x9E3779B9u; // fixed seed: reproducible profiling runs

float NextNoise()
{
    // xorshift32: cheap, deterministic, good enough to keep filter state off
    // zero (denormal paths exercised) without a real audio source.
    gNoiseState ^= gNoiseState << 13;
    gNoiseState ^= gNoiseState >> 17;
    gNoiseState ^= gNoiseState << 5;
    const float u = float(gNoiseState) * (1.0f / 4294967296.0f); // 0..1
    return (u - 0.5f) * 2.0f;                                    // -1..1
}

int gCurrentCorner = 0;

// Per-section cycle split (dsp/ProfileHook.h): the Engine marks each
// section's end; the time since the previous mark goes to that section. DWT
// cycle counter, 480 MHz.
constexpr int kNumSections = iv::prof::kNumSections;
uint32_t      gProfLast = 0;
uint64_t      gProfAcc[kNumSections]{};
uint64_t      gProfPrev[kNumSections]{};
uint32_t      gProfPeak[kNumSections]{}; // worst single block per section, this corner

void ProfMark(int section)
{
    const uint32_t now = DWT->CYCCNT;
    gProfAcc[section] += now - gProfLast;
    gProfLast = now;
}

void StartCycleCounter()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR    = 0xC5ACCE55; // unlock (Cortex-M7)
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

struct Result {
    int      index;
    float    avg, max, min;
    size_t   memBytes;
    uint64_t split[kNumSections];
    uint32_t peak[kNumSections];
    size_t   samples;
};

// The audio runs (codec, SAI and DMA as in release) but plays silence: the Engine is timed in the
// main loop instead, one block after another off the audio clock, so a corner over 100 % still
// reports (timed inside the callback, one over 100 % starved the main loop and the serial stopped,
// M3 round 1, 25 Oct 2026). The callback and USB interrupts still land inside the timed blocks.
// The headroom test (M3 round 9, the owner's idea: find the load where drop-outs really start, not
// SPEC's 75 % guess): after each lap, the heaviest corner runs in the audio callback itself, with
// gPadCycles of busy waiting added, stepped up 5 % at a time. A block that takes longer than its
// period is late: what the codec plays then is a stale half-buffer, a drop-out.
volatile bool     gRealtime = false;
volatile uint32_t gPadCycles = 0, gPeriodCycles = 1, gLate = 0, gBlocks = 0, gMaxCycles = 0, gStepBlocks = 1;
volatile uint64_t gSumCycles = 0;

void AudioCallback(AudioHandle::InputBuffer /*in*/, AudioHandle::OutputBuffer out, size_t size)
{
    if (!gRealtime) {
        for (size_t i = 0; i < size; ++i) out[0][i] = out[1][i] = 0.0f;
        return;
    }
    const uint32_t t0 = DWT->CYCCNT;
    static float buf[kBlockSize];
    for (size_t i = 0; i < size; ++i) {
        float x = 5.0e-4f * NextNoise();
        if (gClickPhase == 0) x += 0.8f;
        if (++gClickPhase >= gClickPeriodSamples) gClickPhase = 0;
        buf[i] = x;
    }
    engine.process(buf, buf, out[0], out[1], int(size));
    const uint32_t pad0 = DWT->CYCCNT;
    while (DWT->CYCCNT - pad0 < gPadCycles) {
    }
    const uint32_t d = DWT->CYCCNT - t0;
    if (d > gMaxCycles) gMaxCycles = d;
    if (d > gPeriodCycles) ++gLate;
    gSumCycles = gSumCycles + d;
    // The callback ends the step itself: over 100 % it runs back to back and the main loop never gets
    // the CPU to stop it (round 9's first run stalled there).
    if (++gBlocks >= gStepBlocks) gRealtime = false;
}

// Micro-tests (M3 round 6), once at boot before the Engine takes its memory: what each building
// block costs on this chip, in tenths of a cycle per operation. The reverb's sections cost ~10x what
// their instructions should (round 4), so: is it the SDRAM (one stream, 32 at once, writes, a delay
// line's read-then-write), internal RAM (AXI, DTCM), or the maths library?
volatile float gSink = 0.0f;
// The AXI tests use gAxi, free until engine.prepare() (it had its own 64 KB array, gAxiTest, until the
// overnight review found that GCC had removed it: never written, so its reads folded to 0.0f and
// "axi32" timed a chain of dependent vadds, 3.0 cycles = the VADD latency, not AXI SRAM).
constexpr int  kMicro = 20;
int            gMicro[kMicro]{};
const char* const kMicroNames[kMicro] = {"sdr1", "sdr32", "sdw32", "sdline8", "axi32", "dtcm32",
                                         "exp", "sin", "pow", "log10", "div", "fdnAXI", "fdnSDRAM",
                                         "tape/smp", "bbd/smp", "radio/smp", "bbdSet/blk", "radioSet/blk",
                                         "loopNeve/smp", "loopMidi/smp"};

// The same small tank (an Fdn at 12 kHz: 0.1 s mean lines, in-loop diffusion and early taps on) run
// with its memory in AXI SRAM and then in SDRAM, cycles per sample (M3 round 8): if they cost about
// the same, the tank is short of arithmetic; if the SDRAM one is far slower, it's the memory.
constexpr size_t kTestFdnFloats = iv::dsp::Fdn::memoryFloats(12000.0f, 0.25f);
constexpr size_t kTestArenaFloats = kTestFdnFloats + iv::dsp::Arena::kSkewLines * iv::dsp::Arena::kLineFloats;
static_assert(kTestArenaFloats <= kAxiFloats, "the AXI test tank lives in gAxi (before engine.prepare)");
alignas(iv::dsp::Fdn) unsigned char gTestFdnStorage[sizeof(iv::dsp::Fdn)];

int TimeTestFdn(float* mem, size_t floats, bool skew)
{
    auto* f = new (gTestFdnStorage) iv::dsp::Fdn();
    iv::dsp::Arena a;
    a.reset(mem, floats, skew);
    if (!f->prepare(a, 12000.0f, 0x1234u, 0.25f)) return -10;
    f->setMeanSeconds(0.1f, 1.0f, true);
    f->setDecayRate(1.0f / 3.0f);
    f->setInLoopDiffusion(1.0f);
    f->setEarlyTaps(1.0f);
    f->reset();
    static float in[32], l[32], r[32];
    uint32_t s = 1;
    auto block = [&] {
        for (int i = 0; i < 32; ++i) {
            s = s * 1664525u + 1013904223u;
            in[i] = 0.1f * float(int32_t(s)) * (1.0f / 2147483648.0f);
            l[i] = r[i] = 0.0f;
        }
        f->process(in, in, l, r, 32);
    };
    for (int b = 0; b < 375; ++b) block(); // 1 s to fill the lines
    const uint32_t t0 = DWT->CYCCNT;
    for (int b = 0; b < 375; ++b) block();
    const int tenths = int(float(DWT->CYCCNT - t0) * 10.0f / (375.0f * 32.0f));
    gSink = l[0];
    return tenths;
}

template <typename F> int Tenths(int ops, F&& f)
{
    const uint32_t t0 = DWT->CYCCNT;
    f();
    return int(float(DWT->CYCCNT - t0) * 10.0f / float(ops));
}

void RunMicroTests()
{
    float* sd = gLarge; // all of it free until engine.prepare()
    float  acc = 0.0f;
    gMicro[0] = Tenths(1 << 18, [&] { for (int i = 0; i < (1 << 18); ++i) acc += sd[i]; });
    // 32 streams, each sequential, 160 KB apart (plus an odd offset), as a tank's reads.
    gMicro[1] = Tenths(32 * 4096, [&] {
        for (int s = 0; s < 4096; ++s)
            for (int k = 0; k < 32; ++k) acc += sd[(1 << 20) + k * 40009 + s];
    });
    gMicro[2] = Tenths(32 * 4096, [&] {
        for (int s = 0; s < 4096; ++s)
            for (int k = 0; k < 32; ++k) sd[(1 << 22) + k * 40009 + s] = float(s);
    });
    // 8 delay lines: read one place, write another (each 0.4 s apart), as the FDN's lines.
    gMicro[3] = Tenths(8 * 4096, [&] {
        for (int s = 0; s < 4096; ++s)
            for (int k = 0; k < 8; ++k) {
                float* line = sd + (3 << 20) + k * 262147; // within gLarge (~7.9 M floats)
                line[s + 19200] = 0.5f * line[s];
            }
    });
    gMicro[4] = Tenths(32 * 512, [&] {
        for (int s = 0; s < 512; ++s)
            for (int k = 0; k < 32; ++k) acc += gAxi[k * 512 + s];
    });
    gMicro[5] = Tenths(32 * 256, [&] {
        for (int s = 0; s < 256; ++s)
            for (int k = 0; k < 32; ++k) acc += gFast[k * 256 + s];
    });
    volatile float x = 0.37f;
    gMicro[6] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::exp(x + float(i) * 1e-4f); });
    gMicro[7] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::sin(x + float(i) * 1e-4f); });
    gMicro[8] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::pow(x, 1.0f + float(i) * 1e-4f); });
    gMicro[9] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::log10(x + float(i) * 1e-4f); });
    gMicro[10] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += 1.0f / (x + float(i)); });
    // ERA's stages alone (M3 round 10), stereo per sample (both channels) or per block's set():
    {
        static iv::dsp::TapeOut tape[2];
        static iv::dsp::BbdStage bbd;
        static iv::dsp::RadioStage radio;
        static iv::dsp::LoopEra loop;
        for (auto& t : tape) { t.reset(); t.set(1.0f, 48000.0f); }
        bbd.reset();
        bbd.set(0.5f, 48000.0f);
        radio.reset();
        radio.set(0.5f, 48000.0f, 32);
        float l = 0.1f, r = -0.1f;
        gMicro[13] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { l = tape[0].process(0.3f * NextNoise()); r = tape[1].process(0.3f * NextNoise()); acc += l + r; } });
        gMicro[14] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { acc += bbd.process(0.3f * NextNoise(), 0) + bbd.process(0.3f * NextNoise(), 1); } });
        gMicro[15] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { l = 0.3f * NextNoise(); r = 0.3f * NextNoise(); radio.process(l, r); acc += l + r; } });
        gMicro[16] = Tenths(100, [&] { for (int i = 0; i < 100; ++i) bbd.set(0.5f + 1e-4f * float(i), 48000.0f); });
        gMicro[17] = Tenths(100, [&] { for (int i = 0; i < 100; ++i) radio.set(0.5f, 48000.0f, 32); });
        loop.prepare(48000.0f);
        loop.set(iv::map::eraMix(0.1f));
        gMicro[18] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) acc += loop.process(0.3f * NextNoise()); });
        loop.set(iv::map::eraMix(0.3f));
        gMicro[19] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) acc += loop.process(0.3f * NextNoise()); });
    }
    gMicro[11] = TimeTestFdn(gAxi, kTestArenaFloats, false);
    gMicro[12] = TimeTestFdn(gLarge, kLargeFloats, true);
    gSink = acc;
}

// One corner: kCornerSeconds of the test signal through the Engine, each block timed by the cycle
// counter against its share of the clock (480 MHz / 48 kHz = 10,000 cycles a sample).
Result MeasureCorner(int index, float fs)
{
    static float bufL[kBlockSize], bufR[kBlockSize], outL[kBlockSize], outR[kBlockSize];
    const float budget = float(SystemCoreClock) / fs * float(kBlockSize); // cycles per block
    Result r{};
    r.index = index;
    r.min = 1e9f;
    float sum = 0.0f;
    int blocks = 0;
    for (int k = 0; k < kNumSections; ++k) gProfAcc[k] = gProfPrev[k] = gProfPeak[k] = 0;
    for (size_t done = 0; done < gCornerSamples; done += kBlockSize) {
        for (int i = 0; i < kBlockSize; ++i) {
            float x = 5.0e-4f * NextNoise(); // low-level noise: keeps loops busy between clicks
            if (gClickPhase == 0) x += 0.8f; // click
            if (++gClickPhase >= gClickPeriodSamples) gClickPhase = 0;
            bufL[i] = bufR[i] = x;
        }
        const uint32_t t0 = DWT->CYCCNT;
        gProfLast = t0;
        engine.process(bufL, bufR, outL, outR, kBlockSize);
        const float load = float(DWT->CYCCNT - t0) / budget;
        for (int k = 0; k < kNumSections; ++k) {
            const uint32_t d = uint32_t(gProfAcc[k] - gProfPrev[k]);
            if (d > gProfPeak[k]) gProfPeak[k] = d;
            gProfPrev[k] = gProfAcc[k];
        }
        sum += load;
        if (load > r.max) r.max = load;
        if (load < r.min) r.min = load;
        ++blocks;
        r.samples += kBlockSize;
    }
    r.avg = blocks ? sum / float(blocks) : 0.0f;
    r.memBytes = engine.memoryBytes();
    for (int k = 0; k < kNumSections; ++k) {
        r.split[k] = gProfAcc[k];
        r.peak[k]  = gProfPeak[k];
    }
    return r;
}

} // namespace

int main()
{
    new (gEngineStorage) iv::Engine(); // before anything touches it (see gEngineStorage)
    // DaisyVersio::Init minus the parts this build never uses (flash; Resilio's
    // flash study, 4 Oct 2026): the knob/CV ADC, the two switches, the button
    // and the gate. Same Seed set-up (clocks at 480 MHz, caches, SDRAM, audio
    // codec and SAI) and the same four LEDs, so the audio callback, and with
    // it the CPU measurement, is unchanged.
    hw.seed.Configure();
    hw.seed.Init(true); // boost to 480 MHz
    SdramWriteAllocate();
    {
        constexpr Pin kLedPins[DaisyVersio::LED_LAST][3] = {
            {seed::D10, seed::D3, seed::D4},
            {seed::D12, seed::D13, seed::D11},
            {seed::D25, seed::D26, seed::D14},
            {seed::D29, seed::D27, seed::D15},
        }; // libDaisy src/daisy_versio.cpp PIN_LEDn_R/G/B
        for (int i = 0; i < DaisyVersio::LED_LAST; ++i)
            hw.leds[i].Init(kLedPins[i][0], kLedPins[i][1], kLedPins[i][2], true);
    }
    hw.seed.SetAudioBlockSize(kBlockSize);

    StartCycleCounter();
    RunMicroTests(); // before the Engine takes (and zeroes) its memory
    engine.attachMemory(gLarge, kLargeFloats, gFast, kFastFloats, gAxi, kAxiFloats);
    engine.prepare(hw.AudioSampleRate(), kBlockSize);
    iv::prof::markHook = ProfMark;
    BuildCornerTable();
    ApplyCorner(0);

    const float fs      = hw.AudioSampleRate();
    gCornerSamples      = size_t(kCornerSeconds * fs);
    gClickPeriodSamples = size_t(0.5f * fs); // click every 500 ms

    BootPattern();

    // hw.seed.StartLog(false) is deliberately NOT used here: StartLog() also
    // prints a "Daisy is online" banner via PrintLine() (logger.cpp), which
    // alone pulls the whole printf chain back in. UsbHandle carries no
    // per-instance state (libs/libDaisy/src/hid/logger_impl.h static_asserts
    // it), so Init() directly on hw.seed.usb_handle is equivalent, minus the
    // banner and its printf dependency.
    hw.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL);
    hw.StartAudio(AudioCallback);

    bool everExceeded = false;
    while (true) {
        {
            const Result r = MeasureCorner(gCurrentCorner, fs);
            gCurrentCorner = (gCurrentCorner + 1) % kNumCorners;
            ApplyCorner(gCurrentCorner);
            const Corner& c = gCorners[r.index];
            const int cyclesPerSample = int(r.avg * 10000.0f + 0.5f); // SPEC §5: 480 MHz / 48 kHz ~= 10,000 cycles/sample

            char        buf[kLineBufSize];
            char*       p   = buf;
            const char* end = buf + sizeof(buf);
            AppendStr(p, end, "CORNER ");
            AppendPadRight(p, end, c.name, 28);
            AppendStr(p, end, " avg ");
            AppendFixed1(p, end, FracToPercentTenths(r.avg), 5);
            AppendStr(p, end, "% max ");
            AppendFixed1(p, end, FracToPercentTenths(r.max), 5);
            AppendStr(p, end, "% min ");
            AppendFixed1(p, end, FracToPercentTenths(r.min), 5);
            AppendStr(p, end, "% | mem ");
            AppendUInt(p, end, unsigned(r.memBytes));
            AppendStr(p, end, " B | block ");
            AppendInt(p, end, kBlockSize);
            AppendStr(p, end, " | fs ");
            AppendInt(p, end, int(fs));
            AppendStr(p, end, " Hz | ~");
            AppendInt(p, end, cyclesPerSample);
            AppendStr(p, end, " cyc/sample\r\n");

            // Where the time goes, each section as % of the whole budget
            // (10,000 cycles per sample): cycles / (samples x 10) = tenths of a %.
            static const char* const kSectionNames[kNumSections] = {"ctl", "in", "shim", "swell", "twin", "tank",
                                                                    "tkctl", "tkloop", "tkout", "tkmix", "tape", "bbd", "radio", "out"};
            // Same buffer, one transmit: the USB CDC send is non-blocking and
            // drops a second call made while the first is still going out.
            AppendStr(p, end, "  SPLIT");
            for (int k = 0; k < kNumSections; ++k) {
                AppendStr(p, end, " ");
                AppendStr(p, end, kSectionNames[k]);
                // (float, not a 64-bit divide: __aeabi_uldivmod is ~850 B of flash)
                AppendFixed1(p, end, r.samples ? int(float(r.split[k]) / (float(r.samples) * 10.0f)) : 0, 5);
            }
            AppendStr(p, end, "  (% of budget)\r\n");
            // The worst single block per section (% of one block's budget):
            // what makes max jump above avg.
            AppendStr(p, end, "  PEAK ");
            for (int k = 0; k < kNumSections; ++k) {
                AppendStr(p, end, " ");
                AppendStr(p, end, kSectionNames[k]);
                AppendFixed1(p, end, int(r.peak[k] / (uint32_t(kBlockSize) * 10u)), 5);
            }
            AppendStr(p, end, "\r\n");
            if (r.index == 0) { // once per pass through the corners: the chip runs as configured
                AppendStr(p, end, "BENCH clock ");
                AppendUInt(p, end, unsigned(SystemCoreClock / 1000000u));
                AppendStr(p, end, " MHz icache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_IC_Msk) ? "on" : "OFF");
                AppendStr(p, end, " dcache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_DC_Msk) ? "on" : "OFF");
                AppendStr(p, end, "\r\nMICRO (cycles per op)");
                for (int k = 0; k < kMicro; ++k) {
                    AppendStr(p, end, " ");
                    AppendStr(p, end, kMicroNames[k]);
                    AppendStr(p, end, " ");
                    AppendFixed1(p, end, gMicro[k], 1);
                }
                AppendStr(p, end, "\r\n");
            }
            TransmitLine(buf, size_t(p - buf));

            if (r.max > 0.75f) everExceeded = true; // SPEC §5 target: <= 75 % peak worst case
        }

        if (gCurrentCorner == 0) { // a lap done: the headroom test, in real time
            // The heaviest corner as it is (pct -1: no padding), then the lightest stepped up with padding
            // until blocks come late twice: where drop-outs start.
            gPeriodCycles = uint32_t(float(SystemCoreClock) / fs * float(kBlockSize));
            int lateSteps = 0;
            for (int pct = -5; pct <= 100 && lateSteps < 2; pct += 5) {
                const int corner = pct < 0 ? kNumCorners - 1 : 0;
                ApplyCorner(corner);
                gPadCycles = pct < 0 ? 0u : uint32_t(float(gPeriodCycles) * float(pct) / 100.0f);
                gLate = gBlocks = gMaxCycles = 0;
                gSumCycles = 0;
                gStepBlocks = uint32_t(3.0f * fs / float(kBlockSize)); // 3 s of blocks
                gRealtime = true;
                while (gRealtime) {
                }
                System::Delay(5);
                const float period = float(gPeriodCycles);
                const float avg = gBlocks ? float(gSumCycles) / float(gBlocks) / period : 0.0f;
                char        hb[160];
                char*       p   = hb;
                const char* end = hb + sizeof(hb);
                AppendStr(p, end, "HEADROOM ");
                AppendStr(p, end, gCorners[corner].name);
                AppendStr(p, end, " +");
                AppendInt(p, end, pct < 0 ? 0 : pct);
                AppendStr(p, end, "% padding: avg ");
                AppendFixed1(p, end, FracToPercentTenths(avg), 5);
                AppendStr(p, end, "% max ");
                AppendFixed1(p, end, FracToPercentTenths(float(gMaxCycles) / period), 5);
                AppendStr(p, end, "% | late blocks ");
                AppendUInt(p, end, unsigned(gLate));
                AppendStr(p, end, " of ");
                AppendUInt(p, end, unsigned(gBlocks));
                AppendStr(p, end, "\r\n");
                TransmitLine(hb, size_t(p - hb));
                if (pct >= 0 && gLate > 0) ++lateSteps;
            }
            gPadCycles = 0;
            ApplyCorner(gCurrentCorner);
        }

        // LED_0: progress through the corner table (brightness = index / count).
        const float v = float(gCurrentCorner) / float(kNumCorners - 1);
        hw.SetLed(DaisyVersio::LED_0, v, v, v);
        // LED_3: red once any corner has exceeded the SPEC §5 75 % target.
        hw.SetLed(DaisyVersio::LED_3, everExceeded ? 1.0f : 0.0f, 0.0f, 0.0f);
        hw.UpdateLeds();
        System::Delay(20);
    }
}

// =============================================================================
#else // IV_MODE_RELEASE
// =============================================================================
// The real instrument (SPEC §3, §6). 7 knobs (+CV) -> ParamSpec Normalised
// values in panel order; SW0 -> SPACE, SW1 -> ATTACK. The button and the gate
// pass every edge to the Engine (Engine::button / gate), which reads the
// gestures itself (tap latches the freeze, hold paints; the gate does the
// same, SPEC §3 "Button + Gate"). All applied at the start of the block
// (offset 0) so they land within one block (1 ms at 48 frames). No USB
// logging (flash, SPEC §5.3).
//
// LEDs (SPEC §3): level meters as Resilio, left pair In L / In R, right pair
// Out L / Out R; brightness follows level (dB scale), colour warms green ->
// amber, red = input near clip or the output limiter pulling down. While
// frozen the output pair shows cool blue under its meter, pulsing slowly
// while painting (LedMeter.h withFreezeTint).

#include "LedMeter.h"
#include "PotEndStops.h"

namespace {

constexpr int kBlockSize = 48; // SPEC §5

// libDaisy quirk: knobs are already flipped for us. Switches: Resilio's M0
// hardware check (29 Sep 2026) confirmed both read 0 (Switch3::POS_LEFT)
// pointing left on the panel, so left = PLATE / NORMAL as SPEC §3 wants and
// neither needs inverting. Immensio's own M0 check confirms it again.
constexpr bool kSpaceSwitchInverted  = false;
constexpr bool kAttackSwitchInverted = false;

// Output calibration from Resilio's M0 passthrough check (29 Sep 2026): with a
// plain in -> out copy, the Versio's analog path came out polarity-inverted
// and 1.17 dB hotter than a patch cable (flat 30 Hz-4 kHz, both channels).
// Undo both on the way out so MIX 0 is indistinguishable from a cable (SPEC
// §7 M0). Both sides of MIX go through it, so the dry/wet balance is
// unchanged. Immensio's M0 check re-measures it on this module.
constexpr float kOutputTrim = -0.874f; // -(10^(-1.17/20))

// P1..P7 -> ParamSpec: the panel layout (SPEC §3).
constexpr iv::ParamId kPotParams[DaisyVersio::KNOB_LAST] = {
    iv::ParamId::Mix,  iv::ParamId::Decay, iv::ParamId::Tone, iv::ParamId::Era,
    iv::ParamId::Size, iv::ParamId::Drift, iv::ParamId::Time,
};

// ---- LED level meters (SPEC §3 LEDs; maths in LedMeter.h) ------------------
// The owner's NE habit: left pair = input, right pair = output.
enum Meter { kInL, kInR, kOutL, kOutR, kNumMeters };

// Meter -> libDaisy LED index. Wanted on the panel, left to right: In L, In
// R, Out L, Out R. Assumes LED_0..LED_3 run left to right (Resilio ran this
// mapping on the same module); if the meters light in the wrong places,
// reorder this one line.
constexpr size_t kMeterLed[kNumMeters] = {
    DaisyVersio::LED_0, // In L  -> panel LED1 (leftmost)
    DaisyVersio::LED_1, // In R  -> panel LED2
    DaisyVersio::LED_2, // Out L -> panel LED3
    DaisyVersio::LED_3, // Out R -> panel LED4 (rightmost)
};

// Written by the audio callback (peak since the main loop last looked), read
// and cleared by the main loop. Single-word float stores are atomic on the
// M7; a block landing between the loop's read and clear is lost, which a
// 1 ms LED can't show anyway.
volatile float gPeak[kNumMeters] = {};
volatile float gLimiterGain      = 1.0f; // lowest Engine::limiterGain() since last read
volatile bool  gFrozen           = false; // Engine::frozen() as of the last block
volatile bool  gPainting         = false; // Engine::painting() as of the last block

// ---- LED PWM by timer + DMA (Resilio, 30 Sep 2026: "LEDs flicker rather than dim")
// libDaisy's software PWM needs UpdateLeds() called at its sample rate
// (1 kHz by default, not settable through DaisyVersio), and the pins then
// only get ~8 uneven steps per 120 Hz period: dim values alias into sparse
// 1 ms flashes. Calling it faster from the main loop doesn't help: the audio
// callback runs in the SAI DMA interrupt (priority 0, nothing can pre-empt
// it) for most of every 1 ms block, freezing any CPU-driven PWM.
//
// So the CPU only writes a table per GPIO port (ivled::fillPwmWords(), one
// 32-bit BSRR word per PWM step, ~1 kHz from the main loop, only when a
// value changed), and DMA copies it to the port's BSRR register on every
// tick of TIM5: 512 steps per period at ~1 kHz, untouched by the CPU load.
// The 12 LED pins sit on 5 ports (A, B, C, D, G) -> 5 DMA streams, each
// paced by its own TIM5 request (update + the 4 compare channels, which only
// raise DMA requests; no timer pins are used). Streams: DMA2 5/6/7 (unused
// by libDaisy) and DMA2 0/1 (libDaisy's DAC streams; the Versio has no DAC).
// No DMA interrupts are enabled, and the audio's own DMA (SAI, ADC) is on
// DMA1, so the audio callback is untouched: its only new neighbour is the
// main loop's table writes. If a stream doesn't start moving, or stops later
// (a bus error clears its enable bit), the loop falls back to libDaisy's
// PWM: flickery, but lit.
//
// Tables live in SRAM1 (.sram1_bss), which libDaisy's MPU setup makes
// non-cacheable for its first 32 KB (sys/system.cpp), so the DMA reads what
// the CPU wrote without cache maintenance, and table writes don't evict the
// audio code's cached data.
constexpr float kPwmHz      = 1000.0f; // PWM periods per second
constexpr int   kNumLeds    = DaisyVersio::LED_LAST;
constexpr int   kMaxPorts   = 5;
constexpr int   kPinsPerLed = 3; // r, g, b

// DaisyVersio's LED pins (libs/libDaisy/src/daisy_versio.cpp, PIN_LEDn_R/G/B;
// not exported), per libDaisy LED index, in r, g, b order.
constexpr Pin kLedPin[kNumLeds][kPinsPerLed] = {
    {seed::D10, seed::D3, seed::D4},
    {seed::D12, seed::D13, seed::D11},
    {seed::D25, seed::D26, seed::D14},
    {seed::D29, seed::D27, seed::D15},
};

DMA_Stream_TypeDef* const kPwmStream[kMaxPorts] = {DMA2_Stream5, DMA2_Stream6, DMA2_Stream7, DMA2_Stream0,
                                                   DMA2_Stream1};
constexpr uint32_t kPwmRequest[kMaxPorts] = {DMA_REQUEST_TIM5_UP, DMA_REQUEST_TIM5_CH1, DMA_REQUEST_TIM5_CH2,
                                             DMA_REQUEST_TIM5_CH3, DMA_REQUEST_TIM5_CH4};

DMA_BUFFER_MEM_SECTION uint32_t gPwmWords[kMaxPorts][ivled::kPwmSteps];

class LedPwm {
public:
    // After hw.Init(); returns false (and leaves libDaisy's PWM in charge) if
    // anything is off.
    bool start()
    {
        // Group the 12 pins by port.
        for (int led = 0; led < kNumLeds; ++led) {
            for (int c = 0; c < kPinsPerLed; ++c) {
                const Pin pin  = kLedPin[led][c];
                int       port = 0;
                while (port < numPorts_ && ports_[port].id != pin.port) ++port;
                if (port == numPorts_) {
                    if (numPorts_ == kMaxPorts) return false;
                    ports_[numPorts_++].id = pin.port;
                }
                Port& p             = ports_[port];
                p.pins[p.numPins]   = ivled::pwmPin(pin.pin, true); // active low
                p.counts[p.numPins] = 0;
                slot_[led][c]       = {port, p.numPins++};
            }
        }
        // The tables must sit in the non-cacheable window (see above).
        const uintptr_t lo = uintptr_t(&gPwmWords[0][0]), hi = lo + sizeof gPwmWords;
        if (lo < 0x30000000u || hi > 0x30008000u) return false;
        for (int p = 0; p < numPorts_; ++p)
            ivled::fillPwmWords(gPwmWords[p], ivled::kPwmSteps, ports_[p].pins, ports_[p].counts, ports_[p].numPins);

        // TIM5 (free: libDaisy's System uses TIM2), one update per PWM step.
        // Its clock is 2x PCLK1 (240 MHz at 480 MHz boost; libDaisy's
        // TimerHandle::GetFreq() uses the same rule), so ~469 ticks a step.
        // The period goes in at Init, which loads it straight away.
        const float ticks = float(System::GetPClk1Freq()) * 2.0f / (kPwmHz * float(ivled::kPwmSteps));
        TimerHandle::Config tc;
        tc.periph = TimerHandle::Config::Peripheral::TIM_5;
        tc.dir    = TimerHandle::Config::CounterDir::UP;
        tc.period = std::max(uint32_t(8), uint32_t(ticks + 0.5f)) - 1;
        if (timer_.Init(tc) != TimerHandle::Result::OK) return false;
        TIM5->CCR1 = 1; // compare events at distinct points of each step,
        TIM5->CCR2 = 2; // each raising one stream's DMA request
        TIM5->CCR3 = 3;
        TIM5->CCR4 = 4;
        TIM5->DIER |= TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE | TIM_DIER_CC3DE | TIM_DIER_CC4DE;

        __HAL_RCC_DMA2_CLK_ENABLE();
        for (int p = 0; p < numPorts_; ++p) {
            DMA_HandleTypeDef& d       = dma_[p];
            d.Instance                 = kPwmStream[p];
            d.Init.Request             = kPwmRequest[p];
            d.Init.Direction           = DMA_MEMORY_TO_PERIPH;
            d.Init.PeriphInc           = DMA_PINC_DISABLE;
            d.Init.MemInc              = DMA_MINC_ENABLE;
            d.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
            d.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
            d.Init.Mode                = DMA_CIRCULAR;
            d.Init.Priority            = DMA_PRIORITY_LOW;
            d.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
            // GPIOA..GPIOK sit 0x400 apart in the same order as GPIOPort.
            GPIO_TypeDef* gpio = reinterpret_cast<GPIO_TypeDef*>(GPIOA_BASE + 0x400u * uint32_t(ports_[p].id));
            if (HAL_DMA_Init(&d) != HAL_OK
                || HAL_DMA_Start(&d, uint32_t(gPwmWords[p]), uint32_t(&gpio->BSRR), ivled::kPwmSteps) != HAL_OK) {
                stop();
                return false;
            }
            ++numStarted_;
        }
        if (timer_.Start() != TimerHandle::Result::OK) {
            stop();
            return false;
        }
        // Every stream must be moving: ~100 us is ~50 steps, well short of
        // one full 512-step lap, so a live stream's counter has changed.
        uint32_t before[kMaxPorts];
        for (int p = 0; p < numPorts_; ++p) before[p] = kPwmStream[p]->NDTR;
        System::DelayUs(100);
        for (int p = 0; p < numPorts_; ++p) {
            if (kPwmStream[p]->NDTR == before[p]) {
                stop();
                return false;
            }
        }
        return true;
    }

    // Hand the pins back to libDaisy's PWM: timer and every stream off.
    void stop()
    {
        TIM5->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE | TIM_DIER_CC3DE | TIM_DIER_CC4DE);
        timer_.Stop();
        for (int p = 0; p < numStarted_; ++p) HAL_DMA_Abort(&dma_[p]);
        numStarted_ = 0;
    }

    // False once any stream has stopped (the DMA clears EN on a bus error).
    bool running() const
    {
        for (int p = 0; p < numPorts_; ++p)
            if (!(kPwmStream[p]->CR & DMA_SxCR_EN)) return false;
        return true;
    }

    // Drive values 0..1 (ivled "drive": cubed here), rebuilding only the
    // tables whose counts changed.
    void set(int led, const ivled::Rgb& c)
    {
        const float v[kPinsPerLed] = {c.r, c.g, c.b};
        for (int k = 0; k < kPinsPerLed; ++k) {
            const Slot s = slot_[led][k];
            const int  n = ivled::pwmCount(v[k]);
            if (ports_[s.port].counts[s.pin] != n) {
                ports_[s.port].counts[s.pin] = n;
                ports_[s.port].dirty         = true;
            }
        }
    }

    void flush()
    {
        for (int p = 0; p < numPorts_; ++p) {
            if (!ports_[p].dirty) continue;
            ivled::fillPwmWords(gPwmWords[p], ivled::kPwmSteps, ports_[p].pins, ports_[p].counts, ports_[p].numPins);
            ports_[p].dirty = false;
        }
    }

private:
    struct Port {
        GPIOPort      id      = PORTX;
        int           numPins = 0;
        bool          dirty   = false;
        ivled::PwmPin pins[kNumLeds * kPinsPerLed];
        int           counts[kNumLeds * kPinsPerLed] = {};
    };
    struct Slot {
        int port = 0, pin = 0;
    };
    Port              ports_[kMaxPorts];
    int               numPorts_   = 0;
    int               numStarted_ = 0;
    Slot              slot_[kNumLeds][kPinsPerLed];
    TimerHandle       timer_;
    DMA_HandleTypeDef dma_[kMaxPorts] = {};
};

LedPwm gLedPwm;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    hw.tap.Debounce(); // ProcessAllControls() only handles knobs

    // Button and gate: every debounced edge to the Engine at the block's
    // start; the Engine reads the gestures (unpatched, the gate reads low and
    // never moves).
    static bool lastGate = false;
    const bool  gate     = hw.Gate();
    if (gate != lastGate) engine.gate(gate, 0);
    lastGate = gate;
    static bool lastButton = false;
    const bool  button     = hw.tap.Pressed();
    if (button != lastButton) engine.button(button, 0);
    lastButton = button;

    for (int p = 0; p < DaisyVersio::KNOB_LAST; ++p)
        engine.setParam(kPotParams[p], ivpot::endStops(hw.GetKnobValue(kPotKnob[p]))); // exact 0 / 1 at the stops

    engine.setParam(iv::ParamId::Space,
                    iv::switchToNormalised(SwitchPosition(DaisyVersio::SW_0, kSpaceSwitchInverted)));
    engine.setParam(iv::ParamId::Attack,
                    iv::switchToNormalised(SwitchPosition(DaisyVersio::SW_1, kAttackSwitchInverted)));

    // Meters: only a per-block abs peak per channel here; all smoothing and
    // colour maths runs in the main loop.
    float inL = 0.0f, inR = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        inL = std::max(inL, std::fabs(in[0][i]));
        inR = std::max(inR, std::fabs(in[1][i]));
    }

    engine.process(in[0], in[1], out[0], out[1], int(size));
    float outL = 0.0f, outR = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        out[0][i] *= kOutputTrim;
        out[1][i] *= kOutputTrim;
        outL = std::max(outL, std::fabs(out[0][i]));
        outR = std::max(outR, std::fabs(out[1][i]));
    }

    gPeak[kInL]  = std::max(float(gPeak[kInL]), inL);
    gPeak[kInR]  = std::max(float(gPeak[kInR]), inR);
    gPeak[kOutL] = std::max(float(gPeak[kOutL]), outL);
    gPeak[kOutR] = std::max(float(gPeak[kOutR]), outR);
    gLimiterGain = std::min(float(gLimiterGain), engine.limiterGain());
    gFrozen      = engine.frozen();
    gPainting    = engine.painting();
}

} // namespace

int main()
{
    new (gEngineStorage) iv::Engine(); // before anything touches it (see gEngineStorage)
    hw.Init(true); // boost to 480 MHz
    SdramWriteAllocate();
    hw.SetAudioBlockSize(kBlockSize);
    engine.attachMemory(gLarge, kLargeFloats, gFast, kFastFloats, gAxi, kAxiFloats);
    engine.prepare(hw.AudioSampleRate(), kBlockSize);

    BootPattern(); // then metering

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Our own PWM from here on (see LedPwm); libDaisy's is only the fallback.
    bool ledDma = gLedPwm.start();

    ivled::LevelMeter meters[kNumMeters];
    uint32_t          lastUs = System::GetUs();
    float             clock  = 0.0f; // seconds, for the paint pulse (only its phase matters)
    while (true) {
        // Elapsed time by unsigned subtraction, so the microsecond clock's
        // wrap (~71.6 min) can't matter.
        const uint32_t nowUs = System::GetUs();
        const float    dt    = float(nowUs - lastUs) * 1.0e-6f;
        lastUs               = nowUs;
        clock += dt;
        if (clock >= 1000.0f) clock -= 1000.0f; // keep float precision; 1000 s is a whole number of pulses

        float peak[kNumMeters];
        for (int m = 0; m < kNumMeters; ++m) {
            peak[m]  = gPeak[m];
            gPeak[m] = 0.0f;
        }
        const float limiterGain = gLimiterGain;
        gLimiterGain            = 1.0f;

        // Red: input near the ADC's full scale (the jack's clip point);
        // output while the Engine's safety limiter pulls the wet down. The
        // limiter is stereo-linked, so both output LEDs go red together.
        const bool limiting = ivled::limiterReducing(limiterGain);
        meters[kInL].update(peak[kInL], ivled::inputNearClip(peak[kInL]), dt);
        meters[kInR].update(peak[kInR], ivled::inputNearClip(peak[kInR]), dt);
        meters[kOutL].update(peak[kOutL], limiting, dt);
        meters[kOutR].update(peak[kOutR], limiting, dt);

        // The meters (and so dt) update about once a millisecond; the PWM
        // runs on its own (DMA) and needs no call per step.
        if (ledDma && !gLedPwm.running()) {
            gLedPwm.stop();
            ledDma = false;
        }
        const bool frozen = gFrozen, painting = gPainting;
        for (int m = 0; m < kNumMeters; ++m) {
            ivled::Rgb c = meters[m].colour();
            if (m == kOutL || m == kOutR) c = ivled::withFreezeTint(c, frozen, painting, clock);
            if (ledDma) gLedPwm.set(int(kMeterLed[m]), c);
            else hw.SetLed(kMeterLed[m], c.r, c.g, c.b);
        }
        if (ledDma) gLedPwm.flush();
        else hw.UpdateLeds();
        System::Delay(1);
    }
}

#endif
