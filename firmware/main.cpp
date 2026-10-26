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
#include "PotEndStops.h"
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
// The cycle counter: the profile times with it; the SDRAM timing change below waits on it.
void StartCycleCounter()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR    = 0xC5ACCE55; // unlock (Cortex-M7)
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// SDRAM at datasheet timing, TRP / TRCD 3 / 3 instead of libDaisy's 16 / 10 (overnight review, lit-m7
// 4.3 Finding A; the reasoning and RM0433's rules at "SDRAM timing" in the profile section). M3 round 11
// (26 Oct 2026): SDRAMCHK 0 / 0 / 0, SDRAM reads 34 -> 17 cycles, a MASSIVE tank sample -25 %. Release
// sets it at boot, right after SdramWriteAllocate(), before anything uses the SDRAM.
constexpr uint32_t kTightTrp = 3, kTightTrcd = 3; // SDCLK cycles

void SdramPrechargeAll()
{
    __DSB();
    FMC_Bank5_6_R->SDCMR = FMC_SDCMR_MODE_1 | FMC_SDCMR_CTB1; // MODE 010: PALL, bank 1, NRFS 0, MRD 0
    __DSB();
    const uint32_t t0 = DWT->CYCCNT;
    while (DWT->CYCCNT - t0 < 1000u) { // ~2 us
    }
}

// cleanCache false: only straight after SdramWriteAllocate() (which cleans the D-cache) with nothing
// having touched the SDRAM since, as release does at boot (the clean costs ~100 B of flash).
void SdramSetTiming(uint32_t sdtr, bool cleanCache = true)
{
    __disable_irq();
    if (cleanCache) SCB_CleanInvalidateDCache();
    __DSB();
    __ISB();
    SdramPrechargeAll();
    FMC_Bank5_6_R->SDTR[0] = sdtr;
    __DSB();
    SdramPrechargeAll();
    __enable_irq();
}

uint32_t SdtrWith(uint32_t base, uint32_t trp, uint32_t trcd)
{
    return (base & ~(FMC_SDTRx_TRP_Msk | FMC_SDTRx_TRCD_Msk)) | ((trp - 1u) << FMC_SDTRx_TRP_Pos)
         | ((trcd - 1u) << FMC_SDTRx_TRCD_Pos);
}

// ITCM (STM32H750IB_flash_itcm.lds; release, and the profile unless PROFILE_ITCM=0): the Core's hot code
// (Engine.o, Fdn.o, Plate.o) is linked to run from ITCM but stored in flash; copy it there before
// anything calls it, first thing in main(). Code in flash (main.o), word by word. M3 round 12 (26 Oct
// 2026): it boots, and every corner is 2.5-3.5 points cheaper than from flash through the I-cache.
#ifndef IV_ITCM
#define IV_ITCM 0
#endif
#if IV_ITCM
extern "C" uint32_t _sitcm_text, _eitcm_text, _siitcm_text;
void CopyItcm()
{
    SCB->ITCMCR |= SCB_ITCMCR_EN_Msk; // on from reset on the H7; make sure
    __DSB();
    __ISB();
    const volatile uint32_t* src = &_siitcm_text;
    for (volatile uint32_t* dst = &_sitcm_text; dst < &_eitcm_text;) *dst++ = *src++;
    __DSB();
    __ISB();
}
#endif

#if defined(IV_MODE_PROFILE) || IV_RELEASE_DIAG
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

// The USB CDC send is non-blocking: it reads the buffer while the host polls (so the buffer must
// outlive the call: callers pass static buffers) and refuses (busy) a send made while the last is
// still going out. So: retry until the transport takes it, for at most 200 ms (no host listening:
// the line is dropped, as before).
void TransmitLine(const char* buf, size_t len)
{
    const uint32_t t0 = System::GetNow();
    while (hw.seed.usb_handle.TransmitInternal(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buf)), len)
               != UsbHandle::Result::OK
           && System::GetNow() - t0 < 200u) {
    }
}
#endif
#endif
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
// depends on startup's .bss clear. DTCM budget (release map): gFast 36,864 + this 24,232 = 61,096 B
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
// - Jitter mode, for rack power (on USB power alone the knob circuits aren't powered and every knob
//   reads full scale): SW0 left and SW1 right turn the LEDs into the jitter readout below. A knob's
//   channel (LED_0 R/G/B = P1-P3, LED_1 R/G/B = P4-P6, LED_2 R = P7) lights if its reading never
//   stayed still for 50 ms in the last second; LED_3 is green if no knob left the +-0.0005 band (the
//   SIZE fix would hold still), red if one did, dim blue for the first second.
// - Knob jitter -> USB serial, once a second (overnight review 26 Oct 2026,
//   section 5.1: a reading that never rests defeats SIZE's rest timer and every
//   memo keyed on exact float equality). Read once per audio block, as the
//   release build reads them (same block size, same libDaisy smoothing), over
//   1 s = 1,000 blocks, pots in panel order P1..P7, hands off the knobs:
//     JPP  peak-to-peak of the reading, x100000 (one 16-bit ADC step ~1.5;
//          the SIZE fix's 0.1 % is ~100)
//     JCHG blocks whose reading differed at all from the block before (of 1000)
//     JRUN longest run of identical readings, in blocks (= ms); SIZE's rest
//          timer needs 50
//     JHYS times the reading left a +-0.0005 band around its last accepted value
//          (the SIZE fix's band in HALL, its tightest: 0 = the fix would hold still)

namespace {

constexpr int kBlockSize = 48;

volatile int      gateEdges      = 0;
volatile int      buttonPresses  = 0;
volatile uint32_t gateFlashUntil = 0;

constexpr int   kJitterBlocks = 1000; // 1 s at 48 kHz / 48
// The SIZE fix's band in knob units: voicing::kSizeHysteresis (1e-3 of log mean length) / log(260 / 35)
// in HALL = 0.000499, the tighter of HALL and MASSIVE (1e-3 / log(2 / 0.35) = 0.000574).
constexpr float kJitterBand   = 0.0005f;

struct Jitter {
    uint32_t pp[DaisyVersio::KNOB_LAST];   // x100000
    uint16_t chg[DaisyVersio::KNOB_LAST];
    uint16_t run[DaisyVersio::KNOB_LAST];
    uint16_t hys[DaisyVersio::KNOB_LAST];
};
Jitter            gJitter;        // the last full second, written by the callback
volatile uint32_t gJitterSeq = 0; // odd while the callback writes gJitter

// Called once per block from the audio callback.
void MeasureJitter()
{
    static float    last[DaisyVersio::KNOB_LAST], lo[DaisyVersio::KNOB_LAST], hi[DaisyVersio::KNOB_LAST];
    static float    held[DaisyVersio::KNOB_LAST];
    static uint16_t chg[DaisyVersio::KNOB_LAST], run[DaisyVersio::KNOB_LAST], best[DaisyVersio::KNOB_LAST];
    static uint16_t hys[DaisyVersio::KNOB_LAST];
    static int      blocks = -1; // -1: first call, take the readings as they are

    for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i) {
        const float v = hw.GetKnobValue(kPotKnob[i]);
        if (blocks < 0) {
            last[i] = lo[i] = hi[i] = held[i] = v;
            continue;
        }
        if (v != last[i]) {
            ++chg[i];
            run[i] = 0;
        } else if (++run[i] > best[i]) {
            best[i] = run[i];
        }
        if (std::fabs(v - held[i]) > kJitterBand) {
            held[i] = v;
            ++hys[i];
        }
        lo[i]   = std::min(lo[i], v);
        hi[i]   = std::max(hi[i], v);
        last[i] = v;
    }
    if (++blocks < kJitterBlocks) return;

    gJitterSeq = gJitterSeq + 1;
    __asm__ volatile("" ::: "memory"); // keep gJitter's writes between the two counts
    for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i) {
        gJitter.pp[i]  = uint32_t((hi[i] - lo[i]) * 100000.0f + 0.5f);
        gJitter.chg[i] = chg[i];
        gJitter.run[i] = uint16_t(best[i] + 1); // a run of n repeats is n + 1 identical blocks
        gJitter.hys[i] = hys[i];
        lo[i] = hi[i] = last[i];
        chg[i] = run[i] = best[i] = hys[i] = 0;
    }
    __asm__ volatile("" ::: "memory");
    gJitterSeq = gJitterSeq + 1;
    blocks     = 0;
}

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
    MeasureJitter();

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

        // The last full second of knob jitter (MeasureJitter), for the serial lines and jitter mode.
        static Jitter   shown{};
        static bool     haveJitter = false, freshJitter = false;
        static uint32_t copiedSeq  = 0;
        const uint32_t  seq        = gJitterSeq;
        if (seq != copiedSeq && (seq & 1u) == 0) {
            __asm__ volatile("" ::: "memory");
            const Jitter j = gJitter;
            __asm__ volatile("" ::: "memory");
            if (gJitterSeq == seq) { // not overwritten while copying
                copiedSeq  = seq;
                shown      = j;
                haveJitter = freshJitter = true;
            }
        }

        if (attack == 2 && space == 0) {
            // Jitter mode (SW0 left, SW1 right; readable on rack power, where serial is off limits): per
            // knob, its LED channel lit if its reading never stayed still for 50 ms in the last second
            // (SIZE's rest timer, kSizeRestSeconds); LED_3 green if no knob left the SIZE fix's +-0.0005
            // band in the last second, red if one did, dim blue until the first second is in.
            float u[DaisyVersio::KNOB_LAST];
            bool  crossed = false;
            for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i) {
                u[i] = haveJitter && shown.run[i] < 50 ? 1.0f : 0.0f;
                crossed |= haveJitter && shown.hys[i] > 0;
            }
            hw.SetLed(DaisyVersio::LED_0, u[0], u[1], u[2]);
            hw.SetLed(DaisyVersio::LED_1, u[3], u[4], u[5]);
            hw.SetLed(DaisyVersio::LED_2, u[6], 0, 0);
            if (!haveJitter) hw.SetLed(DaisyVersio::LED_3, 0, 0, 0.2f);
            else hw.SetLed(DaisyVersio::LED_3, crossed ? 1.0f : 0.0f, crossed ? 0.0f : 1.0f, 0);
            hw.UpdateLeds();
        } else {
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
        }

        const uint32_t now = System::GetNow();
        if (now >= nextPrint) {
            nextPrint = now + 100;
            hw.seed.PrintLine("P %4d %4d %4d %4d %4d %4d %4d | SW %d %d | BTN %d %d | GATE %d %d",
                              Milli(p[0]), Milli(p[1]), Milli(p[2]), Milli(p[3]), Milli(p[4]), Milli(p[5]), Milli(p[6]),
                              space, attack, int(hw.tap.Pressed()), buttonPresses, int(hw.Gate()), gateEdges);
        }

        if (freshJitter) {
            freshJitter    = false;
            const Jitter& j = shown;
            hw.seed.PrintLine("JPP  %5lu %5lu %5lu %5lu %5lu %5lu %5lu", (unsigned long)j.pp[0],
                              (unsigned long)j.pp[1], (unsigned long)j.pp[2], (unsigned long)j.pp[3],
                              (unsigned long)j.pp[4], (unsigned long)j.pp[5], (unsigned long)j.pp[6]);
            hw.seed.PrintLine("JCHG %5d %5d %5d %5d %5d %5d %5d", j.chg[0], j.chg[1], j.chg[2], j.chg[3],
                              j.chg[4], j.chg[5], j.chg[6]);
            hw.seed.PrintLine("JRUN %5d %5d %5d %5d %5d %5d %5d", j.run[0], j.run[1], j.run[2], j.run[3],
                              j.run[4], j.run[5], j.run[6]);
            hw.seed.PrintLine("JHYS %5d %5d %5d %5d %5d %5d %5d", j.hys[0], j.hys[1], j.hys[2], j.hys[3],
                              j.hys[4], j.hys[5], j.hys[6]);
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

void AppendInt(char*& p, const char* end, int v)
{
    if (v < 0 && p < end) *p++ = '-';
    AppendUInt(p, end, unsigned(v < 0 ? -v : v));
}

// Fraction (0..1) -> tenths of a percent, e.g. 0.583f -> 583 ("58.3").
int FracToPercentTenths(float frac) { return int(frac * 1000.0f + 0.5f); }

void AppendHex(char*& p, const char* end, uint32_t v)
{
    AppendStr(p, end, "0x");
    for (int s = 28; s >= 0 && p < end; s -= 4) *p++ = "0123456789abcdef"[(v >> s) & 0xFu];
}

constexpr size_t kLineBufSize  = 1100; // CORNER + SPLIT + PEAK lines, sent in one transmit
constexpr size_t kBenchBufSize = 1600; // BENCH + SDRAM + MICRO (once a lap), its own transmit

// ---- Corner table (SPEC §5, §7 M3) -----------------------------------------
// SPACE PLATE/HALL/MASSIVE x SIZE {0,1} x ERA (5) = 30 corners, all fully wet (MIX 1) so the meter
// sees the Engine, not the dry mix, then the worst cases no corner had measured (overnight review,
// 26 Oct 2026: cost-tank 4.2 and cost-input findings 1, 8, 9): a frozen MASSIVE with the twin
// playing, MASSIVE in SHIMMER, and a MASSIVE whose SIZE keeps moving (the 0.3 s crossfade).
enum CornerKind { kPlain, kFrozen, kShimmerCorner, kSizeMove };
struct Corner {
    int   spacePos; // 0/1/2 -> PLATE/HALL/MASSIVE (Switch3 encoding)
    float decay, size, era;
    float seconds;  // how long it is measured
    int   kind;     // CornerKind
    char  name[40];
};

// M3 round 10: every era (the centre of each one's stretch: Neve, Midiverb, bucket-brigade, cassette,
// AM radio), SIZE 0 / 1, DECAY full.
constexpr float kCornerEras[5] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
const char* const kEraNames[5] = {"neve", "midiverb", "bbd", "cassette", "radio"};
constexpr int kNumGridCorners = 3 * 2 * 5;
constexpr int kHeaviestCorner = kNumGridCorners - 1; // MASSIVE S1 radio: the headroom test's corner
constexpr int kNumCorners = kNumGridCorners + 3;
Corner        gCorners[kNumCorners];

const char* const kSpaceNames[3] = {"PLATE", "HALL", "MASSIVE"};

// The SIZE-move corner flicks SIZE between these every kSizeMoveSeconds: each move starts a 0.3 s
// crossfade once SIZE rests (ADR 0025), so one runs ~20 % of the time.
constexpr float kSizeMoveLow = 0.3f, kSizeMoveHigh = 0.8f, kSizeMoveSeconds = 1.5f;

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
                c.seconds  = 3.0f;
                c.kind     = kPlain;
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
    // The extra corners: MASSIVE, Neve, DECAY full. Frozen: 6 s, so most of it is past the freeze's
    // glide (the twin runs from the press on). The SIZE move: 6 s, four moves. Frozen last (round 12):
    // in round 11 it came first and its twin rang on through SHIMMER, the SIZE move and the headroom
    // test (~14 % each that wasn't theirs); MeasureCorner now runs the engine on, untimed, until the
    // twin stops (since v1.0.37 on its 50 ms level, ~15-24 s sooner than before).
    const struct {
        float size, seconds;
        int   kind;
        const char* name;
    } extra[3] = {
        {1.0f, 3.0f, kShimmerCorner, "MASSIVE S1 neve SHIMMER"},
        {kSizeMoveLow, 6.0f, kSizeMove, "MASSIVE neve SIZE .3<>.8"},
        {1.0f, 6.0f, kFrozen, "MASSIVE S1 neve FROZEN+twin"},
    };
    for (const auto& e : extra) {
        Corner& c  = gCorners[idx++];
        c.spacePos = 2;
        c.decay    = 1.0f;
        c.size     = e.size;
        c.era      = kCornerEras[0];
        c.seconds  = e.seconds;
        c.kind     = e.kind;
        char*       p   = c.name;
        const char* end = c.name + sizeof(c.name) - 1;
        AppendStr(p, end, e.name);
        *p = '\0';
    }
}

void ApplyCorner(int idx)
{
    const Corner& c = gCorners[idx];
    const bool shimmer = c.kind == kShimmerCorner;
    engine.setParam(ParamId::Space, iv::switchToNormalised(c.spacePos));
    engine.setParam(ParamId::Decay, c.decay);
    engine.setParam(ParamId::Size, c.size);
    engine.setParam(ParamId::Era, c.era);
    engine.setParam(ParamId::Mix, 1.0f);
    engine.setParam(ParamId::Tone, 1.0f);
    engine.setParam(ParamId::Drift, 1.0f);
    engine.setParam(ParamId::Time, shimmer ? 0.7f : 1.0f);
    engine.setParam(ParamId::Attack, iv::switchToNormalised(shimmer ? 2 : 1)); // SHIMMER or BLOOM
}

// ---- Synthetic test signal: click every 500 ms + low-level seeded noise ---
size_t          gClickPeriodSamples = 0; // set once fs is known, in main()
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

struct Result {
    int      index;
    float    avg, max, min;
    size_t   memBytes;
    uint64_t split[kNumSections];
    uint32_t peak[kNumSections];
    size_t   samples;
    int      maxBlock;   // the block (from the corner's start) that took longest: what spikes?
    int      twinOnMs;   // frozen corner: how long the twin ran on after the corner (untimed)
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
// M3 round 13 (26 Oct 2026; the owner heard crackle in v1.0.37 with a frozen MASSIVE and with SIZE and
// DECAY high): the real-time test also plays the release's knob sends. gKnobSend: the seven knob
// parameters sent every block, as release's AudioCallback does; gJitterAmp: +-amp of noise on each (the
// rack check found every reading changing all the time, inside +-0.0005); gRtFreeze: a tap-freeze at the
// step's start (press, released 10 blocks later), as MeasureCorner's frozen corner does.
volatile bool  gKnobSend = false, gRtFreeze = false;
volatile float gJitterAmp = 0.0f;
float          gKnobBase[7];
constexpr ParamId kKnobParams[7] = {ParamId::Mix, ParamId::Decay, ParamId::Tone, ParamId::Era,
                                    ParamId::Size, ParamId::Drift, ParamId::Time};
uint32_t       gJitterState = 0x2545F491u;
float NextJitter() // its own xorshift32, -1..1, so the input noise's sequence stays as it was
{
    gJitterState ^= gJitterState << 13;
    gJitterState ^= gJitterState >> 17;
    gJitterState ^= gJitterState << 5;
    return float(gJitterState) * (2.0f / 4294967296.0f) - 1.0f;
}

// The size_moves hunt (26 Oct 2026; the owner, v1.0.40: drop-outs and the red late light "generally when
// changing size in MASSIVE", slow sweeps and quick turns alike): SIZE turned by a scripted hand, read the
// way release reads the knob (its 10 ms one-pole, the 0.003 deadband from the last value sent, the end
// stops), one block = 1 ms. gSizeHand: the move (-1: off). 0 a sweep, 0.2 -> 1 in 2 s and back (5 s);
// 1 quick twists, 0.5 <-> 0.9 in 0.15 s, 0.2 s apart; 2 a slow creep 0.7 -> 1 over 6 s (a 0.003 step every
// ~60 ms: past SIZE's 50 ms rest, so every step crossfades).
volatile int gSizeHand = -1;
float Ease(float a, float b, float u) { u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u); return a + (b - a) * u * u * (3.0f - 2.0f * u); }
float SizeHand(int kind, uint32_t block)
{
    const float t = float(block) * float(kBlockSize) / 48000.0f;
    if (kind == 0) {
        const float u = t - 5.0f * float(int(t / 5.0f));
        return u < 2.5f ? Ease(0.2f, 1.0f, (u - 0.5f) / 2.0f) : Ease(1.0f, 0.2f, (u - 3.0f) / 2.0f);
    }
    if (kind == 1) {
        const float u = t - 0.7f * float(int(t / 0.7f));
        return u < 0.35f ? Ease(0.5f, 0.9f, u / 0.15f) : Ease(0.9f, 0.5f, (u - 0.35f) / 0.15f);
    }
    return 0.7f + 0.3f * (t / 6.0f > 1.0f ? 1.0f : t / 6.0f);
}

void AudioCallback(AudioHandle::InputBuffer /*in*/, AudioHandle::OutputBuffer out, size_t size)
{
    if (!gRealtime) {
        for (size_t i = 0; i < size; ++i) out[0][i] = out[1][i] = 0.0f;
        return;
    }
    const uint32_t t0 = DWT->CYCCNT;
    if (gRtFreeze) {
        if (gBlocks == 0) engine.button(true, 0);
        if (gBlocks == 10) engine.button(false, 0);
    }
    if (gKnobSend)
        for (int k = 0; k < 7; ++k) {
            const float v = gKnobBase[k] + gJitterAmp * NextJitter();
            engine.setParam(kKnobParams[k], v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v));
        }
    if (gSizeHand >= 0) { // release's knob path (AudioCallback, IV_MODE_RELEASE)
        static float smooth = -1.0f, sent = -1.0f;
        if (gBlocks == 0) smooth = sent = -1.0f;
        const float raw = SizeHand(gSizeHand, gBlocks);
        smooth = smooth < 0.0f ? raw : smooth + (1.0f / 11.0f) * (raw - smooth);
        const float v = ivpot::endStops(smooth);
        if (std::fabs(v - sent) > 0.003f || ((v == 0.0f || v == 1.0f) && v != sent)) {
            sent = v;
            engine.setParam(ParamId::Size, v);
        }
    }
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

// Round 13's real-time settings: MASSIVE with the knobs where the owner heard the crackle (SIZE and
// DECAY high, the rest near noon), BLOOM, ERA at the Neve or the cassette. Set from the main loop while
// the callback is idle (gRealtime false), and kept in gKnobBase for the callback's sends.
void ApplyRealtime(float era)
{
    const float v[7] = {0.5f, 0.9f, 0.5f, era, 0.9f, 0.5f, 0.5f}; // MIX DECAY TONE ERA SIZE DRIFT TIME
    for (int k = 0; k < 7; ++k) {
        gKnobBase[k] = v[k];
        engine.setParam(kKnobParams[k], v[k]);
    }
    engine.setParam(ParamId::Space, iv::switchToNormalised(2));
    engine.setParam(ParamId::Attack, iv::switchToNormalised(1));
}

// Untimed blocks from the main loop (callback idle): n of them, or until the twin stops (n < 0, capped at
// 30 s). The input is the lap's low noise.
void RunUntimed(int n, float fs)
{
    static float l[kBlockSize], r[kBlockSize], ol[kBlockSize], orr[kBlockSize];
    const int cap = n >= 0 ? n : int(30.0f * fs) / kBlockSize;
    for (int b = 0; b < cap && (n >= 0 || engine.twinActive()); ++b) {
        for (int i = 0; i < kBlockSize; ++i) l[i] = r[i] = 5.0e-4f * NextNoise();
        engine.process(l, r, ol, orr, kBlockSize);
    }
}

// Micro-tests (M3 round 6), once at boot before the Engine takes its memory: what each building
// block costs on this chip, in tenths of a cycle per operation. The reverb's sections cost ~10x what
// their instructions should (round 4), so: is it the SDRAM (one stream, 32 at once, writes, a delay
// line's read-then-write), internal RAM (AXI, DTCM), or the maths library? The overnight review
// (26 Oct 2026) added the FPU's corners (subnormals, VDIV), a prefetch, the swell, a real-size
// MASSIVE tank with its allpass lines in SDRAM or in AXI SRAM, and the same SDRAM tests again at
// datasheet SDRAM timing ('t' names; see SdramSetTiming). Every read test sums into one register,
// so none can beat the 3-cycle VADD latency: 3.0 means "as fast as the FPU's add", not "free".
volatile float gSink = 0.0f;
// The AXI tests use gAxi, free until engine.prepare() (it had its own 64 KB array, gAxiTest, until the
// overnight review found that GCC had removed it: never written, so its reads folded to 0.0f and
// "axi32" timed a chain of dependent vadds, 3.0 cycles = the VADD latency, not AXI SRAM).
enum Micro {
    kSdr1, kSdr32, kSdw32, kSdline8, kAxi32, kDtcm32, kExp, kSin, kPow, kLog10, kDiv, kFdnAxi, kFdnSdram,
    kTape, kBbd, kRadio, kBbdSet, kRadioSet, kLoopNeve, kLoopMidi,
    // overnight review, 26 Oct 2026
    kSub, kNorm, kSubX, kVdivDep, kVdivThr, kVdivIndep, kPld32, kSwell12, kSwellNS, kSwellW, kFdnMassive,
    kFdnMassiveApAxi, kSdr1t, kSdr32t, kSdw32t, kSdline8t, kPld32t, kFdnSdramt, kFdnMassivet,
    kFdnMassiveApAxit, kMicro
};
int               gMicro[kMicro]{};
const char* const kMicroNames[] = {"sdr1", "sdr32", "sdw32", "sdline8", "axi32", "dtcm32",
                                   "exp", "sin", "pow", "log10", "div", "fdnAXI", "fdnSDRAM",
                                   "tape/smp", "bbd/smp", "radio/smp", "bbdSet/blk", "radioSet/blk",
                                   "loopNeve/smp", "loopMidi/smp",
                                   "sub", "norm", "subX", "vdivDep", "vdivThr", "vdivIndep", "pld32",
                                   "swell12/blk", "swellNS/blk", "swellW/blk", "fdnMassive24k", "fdnMassiveApAxi",
                                   "sdr1t", "sdr32t", "sdw32t", "sdline8t", "pld32t", "fdnSDRAMt",
                                   "fdnMassive24kt", "fdnMassiveApAxit"};
static_assert(sizeof(kMicroNames) / sizeof(kMicroNames[0]) == kMicro, "one name per micro-test, in order");

// ---- FPU flush-to-zero (overnight review, lit-m7 4.5, arch F12) -------------------------------------
// FZ (bit 24) in FPSCR for this thread (main loop: the corners) and in FPDSCR, the FPSCR every
// interrupt's FP context starts from (the audio callback: the headroom test). Release doesn't set it;
// IV_PROFILE_FZ (Makefile PROFILE_FZ, default 1) lets the owner compare.
#ifndef IV_PROFILE_FZ
#define IV_PROFILE_FZ 1
#endif
constexpr uint32_t kFzBit = 1u << 24;
void SetFlushToZero(bool on)
{
    __set_FPSCR(on ? (__get_FPSCR() | kFzBit) : (__get_FPSCR() & ~kFzBit));
    FPU->FPDSCR = on ? (FPU->FPDSCR | kFzBit) : (FPU->FPDSCR & ~kFzBit);
    __ISB();
}

// ---- SDRAM timing (overnight review, lit-m7 4.3 Finding A) ----------------------------------------
// libDaisy (src/dev/sdram.cpp) programs TRP 16 and TRCD 10 SDCLK cycles; the AS4C16M32MSA-6 needs
// tRP = tRCD = 18 ns (2 cycles at the 100 MHz SDCLK this board runs, 3 at 120 MHz), so every change
// of SDRAM row waits ~20 cycles too long, ~96 CPU cycles a line fill. The profile build tries 3 / 3
// (FMC_SDTR1 TRCD bits 27:24, TRP 23:20, each stored as cycles - 1, RM0433 22.9.5); TRC (8), TWR (3)
// and the rest stay libDaisy's, and RM0433's rules hold (TWR >= TRAS - TRCD, TWR >= TRC - TRCD - TRP,
// TWR not > TRCD). Bank 2 is unused: FMC_SDTR2 stays at reset (RM0433: TWR 0xF for an unused bank),
// and TRP / TRC live only in FMC_SDTR1. A precharge-all before and after (RM0433: before changing
// RBURST/RPIPE, "to make sure ongoing operations are complete"), interrupts off, the D-cache cleaned
// first so no write-back is in flight. The H7 has no SDSR busy flag: a PALL takes tRP (<= 16 SDCLK,
// 160 ns), so a 2 us wait covers it. The functions are shared with release (above, after
// SdramWriteAllocate): round 11's SDRAMCHK read 0 / 0 / 0 at 3 / 3, so release runs it at boot too.
#ifndef IV_PROFILE_SDRAM_TIGHT
#define IV_PROFILE_SDRAM_TIGHT 1 // keep 3 / 3 for the corner lap (0: back to libDaisy's after the tests)
#endif
uint32_t           gSdtrLibDaisy = 0;             // FMC_SDTR1 as libDaisy left it

// SDRAMCHK: the integrity test for a timing. 8 MB at 24..32 MB of the SDRAM (gLarge ends at
// ~23.8 MB; nothing else lives there), an address-derived pattern written in order and read back
// scattered (every read a line fill from another SDRAM row: precharge + activate, the timings under
// test), then its complement written scattered (write-allocate fills and dirty write-backs) and read
// back in order. The D-cache is cleaned and invalidated between, so every check reads the chip.
// Returns the number of 32-bit words that came back wrong (of 2 x kChkWords checked).
constexpr uintptr_t kChkBase  = 0xC1800000u;
constexpr uint32_t  kChkWords = (8u << 20) / 4u; // a power of two (the scatter is a permutation of it)

uint32_t ChkPattern(uint32_t i)
{
    uint32_t x = i * 2654435761u;
    return x ^ (x >> 15) ^ (i << 7);
}

uint32_t SdramCheck()
{
    auto* m = reinterpret_cast<volatile uint32_t*>(kChkBase);
    constexpr uint32_t kScatter = 0x9E3779B1u; // odd: k -> k * kScatter mod 2^21 visits every word once
    uint32_t errors = 0;
    for (uint32_t i = 0; i < kChkWords; ++i) m[i] = ChkPattern(i);
    SCB_CleanInvalidateDCache();
    for (uint32_t k = 0; k < kChkWords; ++k) {
        const uint32_t i = (k * kScatter) & (kChkWords - 1u);
        errors += m[i] != ChkPattern(i);
    }
    for (uint32_t k = 0; k < kChkWords; ++k) {
        const uint32_t i = (k * kScatter) & (kChkWords - 1u);
        m[i] = ~ChkPattern(i);
    }
    SCB_CleanInvalidateDCache();
    for (uint32_t i = 0; i < kChkWords; ++i) errors += m[i] != ~ChkPattern(i);
    return errors;
}
uint32_t gChkLibDaisy = 0, gChkTight = 0, gChkLap = 0, gChkLaps = 0; // errors

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

// A real-size MASSIVE tank alone (overnight review): an Fdn at 24 kHz (the Engine's fdn_ rate) with
// kFdnMaxLineSeconds of room, 1.4 s mean lines (SIZE ~1), full in-loop diffusion, 4 early taps a line
// (MASSIVE's), DECAY infinite with the governor (the corners' DECAY 1), Neve ERA; its lines in SDRAM,
// its in-loop allpass lines either in SDRAM too (A, as release was) or in AXI SRAM (B, as now).
// Cycles per tank sample (one tank sample = two output samples at 48 kHz).
alignas(iv::dsp::Fdn) unsigned char gMassiveStorage[2][sizeof(iv::dsp::Fdn)];
iv::dsp::Fdn* gMassive[2] = {};
uint32_t      gMassiveRng = 1;

iv::dsp::Fdn* PrepareMassive(int which, float* lines, size_t lineFloats, float* ap, size_t apFloats)
{
    auto* f = new (gMassiveStorage[which]) iv::dsp::Fdn();
    iv::dsp::Arena a, b;
    a.reset(lines, lineFloats, true);
    if (ap) b.reset(ap, apFloats, true);
    if (!f->prepare(a, 24000.0f, 0x1D5A7u, iv::voicing::kFdnMaxLineSeconds, ap ? &b : nullptr)) return nullptr;
    f->setEarlyTapCount(4);
    f->setMeanSeconds(1.4f, 1.0f, true);
    f->setDecayRate(0.0f);
    f->setGovernor(1.0f);
    f->setInLoopDiffusion(1.0f);
    f->setInLoopScale(1.0f);
    f->setEarlyTaps(1.0f);
    f->setEra(iv::map::eraMix(0.1f));
    f->reset();
    return f;
}

// n blocks of 32 tank samples of low noise (+ a click every 0.5 s); returns tenths of a cycle per
// tank sample when timed.
int RunMassive(iv::dsp::Fdn* f, int blocks)
{
    if (!f) return -10;
    static float in[32], l[32], r[32];
    uint32_t cycles = 0;
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < 32; ++i) {
            gMassiveRng = gMassiveRng * 1664525u + 1013904223u;
            in[i] = 0.05f * float(int32_t(gMassiveRng)) * (1.0f / 2147483648.0f) + ((b % 375) == 0 && i == 0 ? 0.8f : 0.0f);
            l[i] = r[i] = 0.0f;
        }
        const uint32_t t0 = DWT->CYCCNT;
        f->process(in, in, l, r, 32);
        cycles += DWT->CYCCNT - t0;
    }
    gSink = l[0];
    return int(float(cycles) * 10.0f / (float(blocks) * 32.0f));
}

template <typename F> int Tenths(int ops, F&& f)
{
    const uint32_t t0 = DWT->CYCCNT;
    f();
    return int(float(DWT->CYCCNT - t0) * 10.0f / float(ops));
}

// gLarge's layout during the tests (floats; all free until engine.prepare(), which zeroes it):
//   0 .. 0.33 M     fdnSDRAM's tank (TimeTestFdn)       1.05 .. 2.30 M  sdr32 / pld32 (reads)
//   1.50 .. 2.65 M  MASSIVE tank A (lines + allpasses)   3.00 .. 4.07 M  MASSIVE tank B (lines)
//   3.15 .. 5.25 M  sdline8 (writes; after tank B is done)  4.19 .. 5.48 M  sdw32 (writes)
//   4.60 .. 6.05 M  the swell (writes; before tank B)    beyond gLarge (24 MB): SDRAMCHK's 8 MB
constexpr size_t kMassiveA = 1500000, kMassiveB = 3000000, kSwellAt = 4600000;
constexpr size_t kMassiveFloats = iv::dsp::Fdn::memoryFloats(24000.0f) + 16 * iv::dsp::Arena::kSkewLines * iv::dsp::Arena::kLineFloats;
static_assert(kMassiveA + kMassiveFloats <= kMassiveB, "tank A fits below tank B");
static_assert(kMassiveB + kMassiveFloats <= kSwellAt, "tank B ends before the swell");
static_assert(kSwellAt + iv::dsp::Swell::memoryFloats(48000.0f) + 8 * iv::dsp::Arena::kSkewLines * iv::dsp::Arena::kLineFloats
                  <= kLargeFloats, "the swell fits gLarge");
static_assert(sizeof(gLarge) <= kChkBase - 0xC0000000u, "SDRAMCHK's 8 MB lie beyond gLarge");
static_assert(iv::dsp::Fdn::apFloats(24000.0f) + 16 * iv::dsp::Arena::kSkewLines * iv::dsp::Arena::kLineFloats <= kAxiFloats,
              "tank B's allpasses fit gAxi");

// The SDRAM read/write patterns (round 6), reused at the tight timing.
float SdramReads(float* sd, bool prefetch)
{
    float acc = 0.0f;
    for (int s = 0; s < 4096; ++s) {
        const bool pld = prefetch && (s & 7) == 0; // once per 8 floats a stream: its next cache line
        for (int k = 0; k < 32; ++k) {
            const float* q = sd + (1 << 20) + k * 40009 + s;
            if (pld) __builtin_prefetch(q + 8);
            acc += *q;
        }
    }
    return acc;
}

void RunSdramTests(float* sd, float& acc, int sdr1, int sdr32, int sdw32, int sdline8, int pld32)
{
    gMicro[sdr1] = Tenths(1 << 18, [&] { for (int i = 0; i < (1 << 18); ++i) acc += sd[i]; });
    // 32 streams, each sequential, 160 KB apart (plus an odd offset), as a tank's reads.
    gMicro[sdr32] = Tenths(32 * 4096, [&] { acc += SdramReads(sd, false); });
    gMicro[sdw32] = Tenths(32 * 4096, [&] {
        for (int s = 0; s < 4096; ++s)
            for (int k = 0; k < 32; ++k) sd[(1 << 22) + k * 40009 + s] = float(s);
    });
    // 8 delay lines: read one place, write another (each 0.4 s apart), as the FDN's lines.
    gMicro[sdline8] = Tenths(8 * 4096, [&] {
        for (int s = 0; s < 4096; ++s)
            for (int k = 0; k < 8; ++k) {
                float* line = sd + (3 << 20) + k * 262147; // within gLarge (~6.2 M floats)
                line[s + 19200] = 0.5f * line[s];
            }
    });
    // The same 32 reads with a PLD of each stream's next line (the overnight review: is a rolling,
    // one-line-ahead prefetch worth it? Round 7 prefetched a whole block at once, a different thing).
    gMicro[pld32] = Tenths(32 * 4096, [&] { acc += SdramReads(sd, true); });
}

// A one-pole decay (y += a (0 - y), three dependent FP ops a step) from y0, 1000 steps: from 1e-38
// every operand and result is subnormal (FZ off) or zero (FZ on); from 1.0 all are normal.
void OnePole(float y0)
{
    volatile float av = 0.015f, xv = 0.0f;
    const float a = av, x = xv;
    float y = y0;
    for (int i = 0; i < 1000; ++i) y += a * (x - y);
    gSink = y;
}

void RunMicroTests()
{
    float* sd = gLarge; // all of it free until engine.prepare()
    float  acc = 0.0f;
    gSdtrLibDaisy = FMC_Bank5_6_R->SDTR[0];
    RunSdramTests(sd, acc, kSdr1, kSdr32, kSdw32, kSdline8, kPld32);
    gMicro[kAxi32] = Tenths(32 * 512, [&] {
        for (int s = 0; s < 512; ++s)
            for (int k = 0; k < 32; ++k) acc += gAxi[k * 512 + s];
    });
    gMicro[kDtcm32] = Tenths(32 * 256, [&] {
        for (int s = 0; s < 256; ++s)
            for (int k = 0; k < 32; ++k) acc += gFast[k * 256 + s];
    });
    volatile float x = 0.37f;
    gMicro[kExp] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::exp(x + float(i) * 1e-4f); });
    gMicro[kSin] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::sin(x + float(i) * 1e-4f); });
    gMicro[kPow] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::pow(x, 1.0f + float(i) * 1e-4f); });
    gMicro[kLog10] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += std::log10(x + float(i) * 1e-4f); });
    gMicro[kDiv] = Tenths(1000, [&] { for (int i = 0; i < 1000; ++i) acc += 1.0f / (x + float(i)); });
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
        gMicro[kTape] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { l = tape[0].process(0.3f * NextNoise()); r = tape[1].process(0.3f * NextNoise()); acc += l + r; } });
        gMicro[kBbd] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { acc += bbd.process(0.3f * NextNoise(), 0) + bbd.process(0.3f * NextNoise(), 1); } });
        gMicro[kRadio] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) { l = 0.3f * NextNoise(); r = 0.3f * NextNoise(); radio.process(l, r); acc += l + r; } });
        gMicro[kBbdSet] = Tenths(100, [&] { for (int i = 0; i < 100; ++i) bbd.set(0.5f + 1e-4f * float(i), 48000.0f); });
        gMicro[kRadioSet] = Tenths(100, [&] { for (int i = 0; i < 100; ++i) radio.set(0.5f, 48000.0f, 32); });
        loop.prepare(48000.0f);
        loop.set(iv::map::eraMix(0.1f));
        gMicro[kLoopNeve] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) acc += loop.process(0.3f * NextNoise()); });
        loop.set(iv::map::eraMix(0.3f));
        gMicro[kLoopMidi] = Tenths(4800, [&] { for (int i = 0; i < 4800; ++i) acc += loop.process(0.3f * NextNoise()); });
    }
    gMicro[kFdnAxi] = TimeTestFdn(gAxi, kTestArenaFloats, false);
    gMicro[kFdnSdram] = TimeTestFdn(gLarge, kLargeFloats, true);

    // Subnormals: cycles per step (3 FP ops), as built ("FZ on/off" in BENCH) and with FZ flipped.
    volatile float tiny = 1.0e-38f, one = 1.0f;
    gMicro[kSub] = Tenths(1000, [&] { OnePole(tiny); });
    gMicro[kNorm] = Tenths(1000, [&] { OnePole(one); });
    SetFlushToZero(!IV_PROFILE_FZ);
    gMicro[kSubX] = Tenths(1000, [&] { OnePole(tiny); });
    SetFlushToZero(IV_PROFILE_FZ);
    // VDIV.F32: a dependent chain (latency), independent divides (throughput), and independent
    // divides each with 4 independent multiplies (if the divider blocks the FPU, vdivIndep is about
    // vdivThr + 4-5; if independent work hides behind a divide, about vdivThr). Cycles per divide.
    {
        volatile float cv = 1.0001f, kv = 0.99999f, x0 = 0.7f;
        gMicro[kVdivDep] = Tenths(1000, [&] {
            const float c = cv;
            float       q = x0;
            for (int i = 0; i < 1000; ++i) q = c / q;
            gSink = q;
        });
        gMicro[kVdivThr] = Tenths(1000, [&] {
            const float c = cv;
            float       b = 1.0f, s = 0.0f;
            for (int i = 0; i < 1000; ++i) {
                s += c / b;
                b += 1.0f;
            }
            gSink = s;
        });
        gMicro[kVdivIndep] = Tenths(1000, [&] {
            const float c = cv, k = kv;
            float       b = 1.0f, s = 0.0f, m1 = 1.0f, m2 = 1.1f, m3 = 1.2f, m4 = 1.3f;
            for (int i = 0; i < 1000; ++i) {
                s += c / b;
                b += 1.0f;
                m1 *= k;
                m2 *= k;
                m3 *= k;
                m4 *= k;
            }
            gSink = s + m1 + m2 + m3 + m4;
        });
    }
    // The swell (BLOOM at MASSIVE S1: the longest span, 7.5 s), one processBlock of 32 samples, its
    // 12 taps a side: without the smear, with it (BLOOM), and with no taps (the writes alone). The
    // taps' part is swellNS - swellW; 24 taps would cost about swell12 + (swellNS - swellW).
    {
        static iv::dsp::Swell swell;
        iv::dsp::Arena a;
        a.reset(gLarge + kSwellAt, kLargeFloats - kSwellAt, true);
        swell.prepare(a, 48000.0f);
        swell.setAttackSeconds(iv::map::bloomSeconds(1.0f) * iv::map::massiveMass(1.0f), true);
        static float l[32], r[32];
        auto run = [&](int blocks) {
            uint32_t cycles = 0;
            for (int b = 0; b < blocks; ++b) {
                for (int i = 0; i < 32; ++i) l[i] = r[i] = 0.1f * NextNoise();
                swell.control(32);
                const uint32_t t0 = DWT->CYCCNT;
                swell.processBlock(l, r, 32);
                cycles += DWT->CYCCNT - t0;
            }
            gSink = l[0];
            return int(float(cycles) * 10.0f / float(blocks));
        };
        run(17000); // the line full (2^19 writes): the steady-state read path
        gMicro[kSwellNS] = run(1000);
        swell.setSmear(true);
        run(3000); // the smear glides in (150 ms a step, ~2 s)
        gMicro[kSwell12] = run(1000);
        swell.setAttackSeconds(0.0f, true);
        run(10);
        gMicro[kSwellW] = run(1000);
    }
    // The real-size MASSIVE tank, allpasses in SDRAM (A) and in AXI SRAM (B). 4,500 blocks of warm-up
    // (6 s: every line full, so the steady-state read path), then 375 blocks (0.5 s) timed.
    gMassive[0] = PrepareMassive(0, gLarge + kMassiveA, kMassiveB - kMassiveA, nullptr, 0);
    RunMassive(gMassive[0], 4500);
    gMicro[kFdnMassive] = RunMassive(gMassive[0], 375);
    gMassive[1] = PrepareMassive(1, gLarge + kMassiveB, kSwellAt - kMassiveB, gAxi, kAxiFloats);
    RunMassive(gMassive[1], 4500);
    gMicro[kFdnMassiveApAxi] = RunMassive(gMassive[1], 375);

    // SDRAM at datasheet timing: the integrity check at libDaisy's timing (a control), then at 3 / 3,
    // and every SDRAM test again there.
    gChkLibDaisy = SdramCheck();
    SdramSetTiming(SdtrWith(gSdtrLibDaisy, kTightTrp, kTightTrcd));
    gChkTight = SdramCheck();
    RunMassive(gMassive[0], 375); // the tanks run on, no new warm-up needed
    gMicro[kFdnMassivet] = RunMassive(gMassive[0], 375);
    RunMassive(gMassive[1], 375);
    gMicro[kFdnMassiveApAxit] = RunMassive(gMassive[1], 375);
    RunSdramTests(sd, acc, kSdr1t, kSdr32t, kSdw32t, kSdline8t, kPld32t);
    gMicro[kFdnSdramt] = TimeTestFdn(gLarge, kLargeFloats, true);
    if (!IV_PROFILE_SDRAM_TIGHT) SdramSetTiming(gSdtrLibDaisy);
    gSink = acc;
}

// One corner: its `seconds` of the test signal through the Engine, each block timed by the cycle
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
    const Corner& c = gCorners[index];
    const int     nBlocks = int(c.seconds * fs) / kBlockSize;
    const int     moveBlocks = int(kSizeMoveSeconds * fs) / kBlockSize;
    for (int b = 0; b < nBlocks; ++b) {
        if (c.kind == kFrozen) {
            // A press freezes at once (GestureReader); released 10 blocks later. At the end, a tap
            // (press, release a block later: under kTapMaxSeconds) unfreezes for the next corner.
            if (b == 0 || b == nBlocks - 3) engine.button(true, 0);
            if (b == 10 || b == nBlocks - 2) engine.button(false, 0);
        }
        if (c.kind == kSizeMove && b > 0 && b % moveBlocks == 0)
            engine.setParam(ParamId::Size, (b / moveBlocks) % 2 ? kSizeMoveHigh : kSizeMoveLow);
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
        if (load > r.max) {
            r.max      = load;
            r.maxBlock = b;
        }
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
    // After the unfreeze: run on, untimed, until the twin stops (capped at 30 s), so the next corner and
    // the headroom test start without it, and report how long that took.
    if (c.kind == kFrozen) {
        const int cap = int(30.0f * fs) / kBlockSize;
        int       n   = 0;
        for (; n < cap && engine.twinActive(); ++n) {
            for (int i = 0; i < kBlockSize; ++i) bufL[i] = bufR[i] = 5.0e-4f * NextNoise();
            engine.process(bufL, bufR, outL, outR, kBlockSize);
        }
        r.twinOnMs = int(float(n) * float(kBlockSize) * 1000.0f / fs);
    }
    return r;
}


} // namespace

int main()
{
#if IV_ITCM
    CopyItcm(); // first: the Engine's constructor may live in Engine.o
#endif
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
    SetFlushToZero(IV_PROFILE_FZ); // before any FP work (the micro-tests flip it once and put it back)

    StartCycleCounter();
    RunMicroTests(); // before the Engine takes (and zeroes) its memory
    engine.attachMemory(gLarge, kLargeFloats, gFast, kFastFloats, gAxi, kAxiFloats);
    engine.prepare(hw.AudioSampleRate(), kBlockSize);
    iv::prof::markHook = ProfMark;
    BuildCornerTable();
    ApplyCorner(0);

    const float fs      = hw.AudioSampleRate();
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

            static char buf[kLineBufSize]; // static: the USB send reads it after TransmitLine returns
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
            AppendStr(p, end, " cyc/sample | max at block ");
            AppendInt(p, end, r.maxBlock);
            if (c.kind == kFrozen) {
                AppendStr(p, end, " | twin on ");
                AppendInt(p, end, r.twinOnMs);
                AppendStr(p, end, " ms after");
            }
            AppendStr(p, end, "\r\n");

            // Where the time goes, each section as % of the whole budget
            // (10,000 cycles per sample): cycles / (samples x 10) = tenths of a %.
            static const char* const kSectionNames[kNumSections] = {"ctl", "in", "shim", "swell", "twin", "tank",
                                                                    "tkctl", "tkloop", "tkout", "tkmix", "tape", "bbd", "radio", "out"};
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
            TransmitLine(buf, size_t(p - buf));
            if (r.index == 0) { // once per pass through the corners: the chip runs as configured
                static char bb[kBenchBufSize];
                p   = bb;
                end = bb + sizeof(bb);
                AppendStr(p, end, "BENCH clock ");
                AppendUInt(p, end, unsigned(SystemCoreClock / 1000000u));
                AppendStr(p, end, " MHz icache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_IC_Msk) ? "on" : "OFF");
                AppendStr(p, end, " dcache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_DC_Msk) ? "on" : "OFF");
                // As the registers are now, not as built: FZ in this thread's FPSCR and in FPDSCR
                // (the audio callback's), the SDRAM's TRP / TRCD in SDCLK cycles and the SDCLK.
                AppendStr(p, end, " | FZ ");
                AppendStr(p, end, (__get_FPSCR() & kFzBit) ? "on" : "off");
                AppendStr(p, end, " (irq ");
                AppendStr(p, end, (FPU->FPDSCR & kFzBit) ? "on" : "off");
                const uint32_t sdtr = FMC_Bank5_6_R->SDTR[0];
                AppendStr(p, end, ") | sdram TRP ");
                AppendUInt(p, end, ((sdtr & FMC_SDTRx_TRP_Msk) >> FMC_SDTRx_TRP_Pos) + 1u);
                AppendStr(p, end, " TRCD ");
                AppendUInt(p, end, ((sdtr & FMC_SDTRx_TRCD_Msk) >> FMC_SDTRx_TRCD_Pos) + 1u);
                AppendStr(p, end, sdtr == gSdtrLibDaisy ? " (libDaisy)" : " (tight)");
                PLL2_ClocksTypeDef pll2;
                HAL_RCCEx_GetPLL2ClockFreq(&pll2);
                const uint32_t sdDiv = (FMC_Bank5_6_R->SDCR[0] >> 10) & 3u; // SDCLK: 2 or 3 FMC clocks
                AppendStr(p, end, " sdclk ");
                AppendUInt(p, end, sdDiv ? pll2.PLL2_R_Frequency / sdDiv / 1000000u : 0u);
                AppendStr(p, end, " MHz | itcm ");
#if IV_ITCM
                AppendUInt(p, end, unsigned(uintptr_t(&_eitcm_text) - uintptr_t(&_sitcm_text)));
                AppendStr(p, end, " B");
#else
                AppendStr(p, end, "off");
#endif
                AppendStr(p, end, " | engine ");
                AppendHex(p, end, uint32_t(reinterpret_cast<uintptr_t>(gEngineStorage)));
                AppendStr(p, end, " axi ");
                AppendHex(p, end, uint32_t(reinterpret_cast<uintptr_t>(gAxi)));
                AppendStr(p, end, "\r\nSDRAMCHK libdaisy ");
                AppendUInt(p, end, gChkLibDaisy);
                AppendStr(p, end, " tight ");
                AppendUInt(p, end, gChkTight);
                AppendStr(p, end, " laps ");
                AppendUInt(p, end, gChkLap);
                AppendStr(p, end, " in ");
                AppendUInt(p, end, gChkLaps);
                AppendStr(p, end, " (wrong words, of ");
                AppendUInt(p, end, 2u * kChkWords);
                AppendStr(p, end, " a check)\r\nMICRO (cycles per op)");
                for (int k = 0; k < kMicro; ++k) {
                    AppendStr(p, end, " ");
                    AppendStr(p, end, kMicroNames[k]);
                    AppendStr(p, end, " ");
                    AppendFixed1(p, end, gMicro[k], 1);
                }
                AppendStr(p, end, "\r\n");
                TransmitLine(bb, size_t(p - bb));
            }

            if (r.max > 0.75f) everExceeded = true; // SPEC §5 target: <= 75 % peak worst case
        }

        if (gCurrentCorner == 0) { // a lap done: the SDRAM check again, at the lap's timing (the module warm)
            gChkLap += SdramCheck();
            ++gChkLaps;
        }
        if (gCurrentCorner == 0) { // a lap done: the headroom test, in real time
            // The heaviest corner as it is (pct -1: no padding), then the lightest stepped up with padding
            // until blocks come late twice: where drop-outs start.
            gPeriodCycles = uint32_t(float(SystemCoreClock) / fs * float(kBlockSize));
            int lateSteps = 0;
            for (int pct = -5; pct <= 100 && lateSteps < 2; pct += 5) {
                const int corner = pct < 0 ? kHeaviestCorner : 0;
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
                static char hb[160];
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

            // Round 13: the release's case in real time. Four settings (MASSIVE knobs high; Neve or
            // cassette; playing or frozen), each with the knobs still and with jitter, 3 s each (frozen 6 s:
            // the freeze glides in first). Between settings the Engine runs on, untimed, until the twin stops.
            struct Rt {
                float       era;
                bool        frozen;
                const char* name;
            };
            static const Rt kRt[4] = {{0.1f, false, "MASSIVE knobs high neve"},
                                      {0.7f, false, "MASSIVE knobs high cassette"},
                                      {0.1f, true, "MASSIVE knobs high neve FROZEN"},
                                      {0.7f, true, "MASSIVE knobs high cassette FROZEN"}};
            for (const Rt& rt : kRt) {
                for (int jit = 0; jit < 2; ++jit) {
                    ApplyRealtime(rt.era);
                    RunUntimed(int(1.0f * fs) / kBlockSize, fs); // 1 s to settle the knobs' glides
                    gKnobSend  = true;
                    gJitterAmp = jit ? 2.0e-4f : 0.0f;
                    gRtFreeze  = rt.frozen;
                    gLate = gBlocks = gMaxCycles = 0;
                    gSumCycles = 0;
                    gStepBlocks = uint32_t((rt.frozen ? 6.0f : 3.0f) * fs / float(kBlockSize));
                    gRealtime = true;
                    while (gRealtime) {
                    }
                    gKnobSend = gRtFreeze = false;
                    gJitterAmp = 0.0f;
                    System::Delay(5);
                    const float period = float(gPeriodCycles);
                    const float avg = gBlocks ? float(gSumCycles) / float(gBlocks) / period : 0.0f;
                    static char rb[160];
                    char*       p   = rb;
                    const char* end = rb + sizeof(rb);
                    AppendStr(p, end, "REALTIME ");
                    AppendPadRight(p, end, rt.name, 34);
                    AppendStr(p, end, jit ? " jitter" : " still ");
                    AppendStr(p, end, " avg ");
                    AppendFixed1(p, end, FracToPercentTenths(avg), 5);
                    AppendStr(p, end, "% max ");
                    AppendFixed1(p, end, FracToPercentTenths(float(gMaxCycles) / period), 5);
                    AppendStr(p, end, "% | late blocks ");
                    AppendUInt(p, end, unsigned(gLate));
                    AppendStr(p, end, " of ");
                    AppendUInt(p, end, unsigned(gBlocks));
                    AppendStr(p, end, "\r\n");
                    TransmitLine(rb, size_t(p - rb));
                    if (rt.frozen) { // a tap unfreezes; run on until the twin stops
                        engine.button(true, 0);
                        RunUntimed(1, fs);
                        engine.button(false, 0);
                        RunUntimed(-1, fs);
                    }
                }
            }

            // The size_moves hunt: SIZE turned by hand (SizeHand) in MASSIVE with the knobs high, Neve and
            // cassette, with the crossfade in 2 groups (as built, v1.0.39), 4 and 8 (the prototypes). Three
            // states: playing; frozen (the pad holds its SIZE, the twin follows the knob and crossfades); just
            // unfrozen (the pad jumps to the knob's SIZE and crossfades while the twin rings on and crossfades
            // too; the owner: drop-outs mostly frozen, "very occasionally when unfrozen too"). The frozen ones
            // the sweep and the twists, just unfrozen the twists.
            static const char* const kHandNames[3] = {"sweep", "twists", "creep"};
            static const char* const kStateNames[3] = {"", "FROZEN ", "UNFROZE "};
            static const int kGroups[3] = {2, 4, 8};
            auto tap = [&](int hold) { // a press, released `hold` blocks later
                engine.button(true, 0);
                RunUntimed(hold, fs);
                engine.button(false, 0);
            };
            for (int state = 0; state < 3; ++state)
                for (int era = 0; era < 2; ++era)
                    for (int hand = 0; hand < 3; ++hand)
                        for (int gi = 0; gi < 3; ++gi) {
                            if ((state > 0 && hand == 2) || (state == 2 && hand == 0)) continue;
                            ApplyRealtime(era ? 0.7f : 0.1f);
                            engine.setSizeStagger(kGroups[gi]);
                            engine.setParam(ParamId::Size, ivpot::endStops(SizeHand(hand, 0)));
                            RunUntimed(int(3.0f * fs) / kBlockSize, fs); // the move to the start, its crossfades done
                            if (state > 0) { // freeze (the twin starts), let the freeze's glide settle
                                tap(10);
                                RunUntimed(int(2.0f * fs) / kBlockSize, fs);
                            }
                            if (state == 2) tap(1); // unfreeze: the twin rings on through the timed moves
                            gLate = gBlocks = gMaxCycles = 0;
                            gSumCycles = 0;
                            gStepBlocks = uint32_t((hand == 2 ? 6.0f : 5.0f) * fs / float(kBlockSize));
                            gSizeHand = hand;
                            gRealtime = true;
                            while (gRealtime) {
                            }
                            gSizeHand = -1;
                            System::Delay(5);
                            const bool twin = engine.twinActive();
                            if (state == 1) tap(1); // unfreeze
                            if (state > 0) RunUntimed(-1, fs); // run on until the twin stops
                            const float period = float(gPeriodCycles);
                            const float avg = gBlocks ? float(gSumCycles) / float(gBlocks) / period : 0.0f;
                            static char sb[160];
                            char*       p   = sb;
                            const char* end = sb + sizeof(sb);
                            AppendStr(p, end, "SIZEHAND MASSIVE ");
                            AppendStr(p, end, kStateNames[state]);
                            AppendStr(p, end, era ? "cassette " : "neve     ");
                            AppendPadRight(p, end, kHandNames[hand], 7);
                            AppendStr(p, end, " groups ");
                            AppendInt(p, end, kGroups[gi]);
                            AppendStr(p, end, " avg ");
                            AppendFixed1(p, end, FracToPercentTenths(avg), 5);
                            AppendStr(p, end, "% max ");
                            AppendFixed1(p, end, FracToPercentTenths(float(gMaxCycles) / period), 5);
                            AppendStr(p, end, "% | late blocks ");
                            AppendUInt(p, end, unsigned(gLate));
                            AppendStr(p, end, " of ");
                            AppendUInt(p, end, unsigned(gBlocks));
                            if (state == 2) AppendStr(p, end, twin ? " | twin still on" : " | twin stopped");
                            AppendStr(p, end, "\r\n");
                            TransmitLine(sb, size_t(p - sb));
                        }
            engine.setSizeStagger(2);
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
#if IV_RELEASE_DIAG
#include "dsp/ProfileHook.h"
#endif

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
// Late blocks (diagnostic, 26 Oct 2026: the owner heard crackle in MASSIVE at noon that round 13's
// real-time test didn't show; RELEASE_LATE_LIGHT=1 builds it, immensio_versio-latelight.bin): the
// callback times itself; a block over its period (48 samples at
// 48 kHz) counts in gLateBlocks, one over 90 % in gNearBlocks. The main loop flashes the rightmost
// LED red for a late block, amber for a near one, so a crackle with a flash is the CPU, one without
// isn't.
volatile uint32_t gLateBlocks = 0, gNearBlocks = 0;

#if IV_RELEASE_DIAG
// RELEASE_DIAG=1 (diagnostic, 26 Oct 2026): the playing build on USB power, reporting over serial. The
// knobs are held at noon and SPACE at MASSIVE, ATTACK NORMAL (on USB power the knob circuits read full
// scale); the input is made here, changing every 10 s: silence, fast notes (8 a second, decaying saws
// at 0.5), the profile's test signal (quiet noise and a click every 500 ms). Once a second: the
// callback's and the Engine's average and worst block (% of the 1 ms period), late / near blocks, and
// each section's worst block (prof::Section, as the profile's PEAK line).
constexpr int     kDiagInputs = 3;
const char* const kDiagInputNames[kDiagInputs] = {"silence", "fast notes", "test signal"};
volatile int      gDiagInput = 0;
volatile uint32_t gDiagCbMax = 0, gDiagEngMax = 0, gDiagBlocks = 0;
volatile uint64_t gDiagCbSum = 0, gDiagEngSum = 0;
uint32_t          gDiagSecPeak[iv::prof::kNumSections];
uint32_t          gDiagSecAcc[iv::prof::kNumSections], gDiagLast = 0;
void DiagMark(int section)
{
    const uint32_t now = DWT->CYCCNT;
    gDiagSecAcc[section] += now - gDiagLast;
    gDiagLast = now;
}
uint32_t gDiagRng = 0x1234567u;
float DiagNoise()
{
    gDiagRng ^= gDiagRng << 13;
    gDiagRng ^= gDiagRng >> 17;
    gDiagRng ^= gDiagRng << 5;
    return float(gDiagRng) * (2.0f / 4294967296.0f) - 1.0f;
}
// One input sample. Fast notes: a new note every 6,000 samples, a saw at a pitch from a fixed list,
// decaying with a 60 ms time constant.
float DiagInput(int kind)
{
    static uint32_t n = 0, sinceNote = 0, note = 0;
    static float    phase = 0.0f, env = 0.0f;
    ++n;
    if (kind == 0) return 0.0f;
    if (kind == 2) return 5.0e-4f * DiagNoise() + (n % 24000u == 0 ? 0.8f : 0.0f);
    constexpr float kHz[8] = {110.0f, 220.0f, 165.0f, 330.0f, 247.0f, 440.0f, 196.0f, 587.0f};
    if (++sinceNote >= 6000u) {
        sinceNote = 0;
        note      = (note + 3u) % 8u;
        env       = 0.5f;
    }
    phase += kHz[note] / 48000.0f;
    if (phase >= 1.0f) phase -= 1.0f;
    env *= 0.99965f; // ~60 ms
    return env * (2.0f * phase - 1.0f);
}
#endif
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
#if IV_LATE_LIGHT || IV_RELEASE_DIAG
    const uint32_t t0 = DWT->CYCCNT;
#endif
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

    // Knob readings (26 Oct 2026). The rack jitter check found every reading changing all the time, and
    // with the reverb running the audio gets into them: playing notes moved SIZE's reading past the old
    // 0.0005 deadband, and each move started a SIZE crossfade in MASSIVE (up to ~110 % of the CPU): the
    // owner's drop-outs, and never in PLATE, which glides. So each reading is smoothed by a one-pole of
    // kKnobSmoothMs (on top of libDaisy's 2 ms) and reaches the Engine only once it has moved more than
    // its deadband from the last value sent, or at an end stop (exact 0 / 1): kKnobDeadband for most,
    // kCrossfadeDeadband for SIZE and TIME, whose changes start a crossfade in HALL and MASSIVE (0.3 % of
    // the travel: ~9 cents of MASSIVE's room, 2 ms of pre-delay; they jump by crossfade anyway).
    constexpr float kKnobSmoothMs      = 10.0f;
    constexpr float kKnobSmooth        = 1.0f / (1.0f + kKnobSmoothMs * 48.0f / float(kBlockSize)); // a block = 1 ms
    constexpr float kKnobDeadband      = 0.001f;
    constexpr float kCrossfadeDeadband = 0.003f;
    static float    smooth[DaisyVersio::KNOB_LAST] = {-1, -1, -1, -1, -1, -1, -1};
    static float    sent[DaisyVersio::KNOB_LAST]   = {-1, -1, -1, -1, -1, -1, -1};
    for (int p = 0; p < DaisyVersio::KNOB_LAST; ++p) {
#if IV_RELEASE_DIAG
        const float raw = 0.5f; // every knob at noon
#else
        const float raw = hw.GetKnobValue(kPotKnob[p]);
#endif
        smooth[p] = smooth[p] < 0.0f ? raw : smooth[p] + kKnobSmooth * (raw - smooth[p]);
        const float v    = ivpot::endStops(smooth[p]); // exact 0 / 1 at the stops
        const bool  xf   = kPotParams[p] == iv::ParamId::Size || kPotParams[p] == iv::ParamId::Time;
        const float band = xf ? kCrossfadeDeadband : kKnobDeadband;
        if (std::fabs(v - sent[p]) > band || ((v == 0.0f || v == 1.0f) && v != sent[p])) {
            sent[p] = v;
            engine.setParam(kPotParams[p], v);
        }
    }

#if IV_RELEASE_DIAG
    engine.setParam(iv::ParamId::Space, iv::switchToNormalised(2));
    engine.setParam(iv::ParamId::Attack, iv::switchToNormalised(0));
    static float dl[kBlockSize], dr[kBlockSize];
    const int    kind = gDiagInput;
    for (size_t i = 0; i < size; ++i) dl[i] = dr[i] = DiagInput(kind);
    const float* srcL = dl;
    const float* srcR = dr;
#else
    engine.setParam(iv::ParamId::Space,
                    iv::switchToNormalised(SwitchPosition(DaisyVersio::SW_0, kSpaceSwitchInverted)));
    engine.setParam(iv::ParamId::Attack,
                    iv::switchToNormalised(SwitchPosition(DaisyVersio::SW_1, kAttackSwitchInverted)));
    const float* srcL = in[0];
    const float* srcR = in[1];
#endif

    // Meters: only a per-block abs peak per channel here; all smoothing and
    // colour maths runs in the main loop.
    float inL = 0.0f, inR = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        inL = std::max(inL, std::fabs(srcL[i]));
        inR = std::max(inR, std::fabs(srcR[i]));
    }

#if IV_RELEASE_DIAG
    for (int k = 0; k < iv::prof::kNumSections; ++k) gDiagSecAcc[k] = 0;
    const uint32_t e0 = DWT->CYCCNT;
    gDiagLast = e0;
#endif
    engine.process(srcL, srcR, out[0], out[1], int(size));
#if IV_RELEASE_DIAG
    const uint32_t eng = DWT->CYCCNT - e0;
    for (int k = 0; k < iv::prof::kNumSections; ++k)
        if (gDiagSecAcc[k] > gDiagSecPeak[k]) gDiagSecPeak[k] = gDiagSecAcc[k];
#endif
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

#if IV_LATE_LIGHT || IV_RELEASE_DIAG
    const uint32_t period = uint32_t(480u * 1000u * kBlockSize / 48u); // CPU cycles a block at 480 MHz, 48 kHz
    const uint32_t d      = DWT->CYCCNT - t0;
    if (d > period) gLateBlocks = gLateBlocks + 1;
    else if (d > period / 10u * 9u) gNearBlocks = gNearBlocks + 1;
#endif
#if IV_RELEASE_DIAG
    if (d > gDiagCbMax) gDiagCbMax = d;
    if (eng > gDiagEngMax) gDiagEngMax = eng;
    gDiagCbSum   = gDiagCbSum + d;
    gDiagEngSum  = gDiagEngSum + eng;
    gDiagBlocks  = gDiagBlocks + 1;
#endif
}

} // namespace

int main()
{
    CopyItcm(); // first: the Engine's constructor may live in Engine.o
    new (gEngineStorage) iv::Engine(); // before anything touches it (see gEngineStorage)
    hw.Init(true); // boost to 480 MHz
    SdramWriteAllocate();
    StartCycleCounter();
#if !defined(IV_RELEASE_SDRAM_TIGHT) || IV_RELEASE_SDRAM_TIGHT
    SdramSetTiming(SdtrWith(FMC_Bank5_6_R->SDTR[0], kTightTrp, kTightTrcd), false); // M3 round 11: SDRAMCHK clean
#endif
    hw.SetAudioBlockSize(kBlockSize);
    engine.attachMemory(gLarge, kLargeFloats, gFast, kFastFloats, gAxi, kAxiFloats);
    engine.prepare(hw.AudioSampleRate(), kBlockSize);

    BootPattern(); // then metering

    hw.StartAdc();
#if IV_RELEASE_DIAG
    hw.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL); // serial without the logger's printf (as profile)
    iv::prof::markHook = DiagMark;
    uint32_t diagNext = System::GetNow() + 1000u, diagSeconds = 0;
#endif
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
#if IV_LATE_LIGHT
        // Late / near blocks (diagnostic): the rightmost LED red / amber for 150 ms after one.
        static uint32_t seenLate = 0, seenNear = 0;
        static float    lateUntil = -1.0f, nearUntil = -1.0f;
        const uint32_t  late = gLateBlocks, near = gNearBlocks;
        if (late != seenLate) lateUntil = clock + 0.15f;
        if (near != seenNear) nearUntil = clock + 0.15f;
        seenLate = late;
        seenNear = near;
        if (lateUntil > clock + 1.0f || nearUntil > clock + 1.0f) lateUntil = nearUntil = -1.0f; // clock wrapped
#endif
        for (int m = 0; m < kNumMeters; ++m) {
            ivled::Rgb c = meters[m].colour();
            if (m == kOutL || m == kOutR) c = ivled::withFreezeTint(c, frozen, painting, clock);
#if IV_LATE_LIGHT
            if (kMeterLed[m] == DaisyVersio::LED_3 && clock < lateUntil) c = {1.0f, 0.0f, 0.0f};
            else if (kMeterLed[m] == DaisyVersio::LED_3 && clock < nearUntil) c = {1.0f, 0.4f, 0.0f};
#endif
            if (ledDma) gLedPwm.set(int(kMeterLed[m]), c);
            else hw.SetLed(kMeterLed[m], c.r, c.g, c.b);
        }
        if (ledDma) gLedPwm.flush();
        else hw.UpdateLeds();
#if IV_RELEASE_DIAG
        if (System::GetNow() >= diagNext) {
            diagNext += 1000u;
            const uint32_t blocks = gDiagBlocks ? gDiagBlocks : 1u;
            const float    per    = 480000.0f * float(kBlockSize) / 48.0f / 100.0f; // cycles per 1 % of a block
            auto t = [per](float cycles) { return int(cycles / per * 10.0f + 0.5f); }; // tenths of a %
            static char line[400];
            char*       q   = line;
            const char* end = line + sizeof(line);
            AppendStr(q, end, "DIAG ");
            AppendStr(q, end, kDiagInputNames[gDiagInput]);
            AppendStr(q, end, " | cb avg");
            AppendFixed1(q, end, t(float(gDiagCbSum / blocks)), 6);
            AppendStr(q, end, " max");
            AppendFixed1(q, end, t(float(gDiagCbMax)), 6);
            AppendStr(q, end, " | engine avg");
            AppendFixed1(q, end, t(float(gDiagEngSum / blocks)), 6);
            AppendStr(q, end, " max");
            AppendFixed1(q, end, t(float(gDiagEngMax)), 6);
            AppendStr(q, end, " | late ");
            AppendUInt(q, end, unsigned(gLateBlocks));
            AppendStr(q, end, " near ");
            AppendUInt(q, end, unsigned(gNearBlocks));
            AppendStr(q, end, "\r\n  PEAK");
            static const char* const kSec[iv::prof::kNumSections] = {"ctl", "in", "shim", "swell", "twin", "tank", "tkctl",
                                                                     "tkloop", "tkout", "tkmix", "tape", "bbd", "radio", "out"};
            for (int k = 0; k < iv::prof::kNumSections; ++k) {
                AppendStr(q, end, " ");
                AppendStr(q, end, kSec[k]);
                AppendFixed1(q, end, t(float(gDiagSecPeak[k])), 5);
                gDiagSecPeak[k] = 0;
            }
            AppendStr(q, end, "\r\n");
            TransmitLine(line, size_t(q - line));
            gDiagCbMax = gDiagEngMax = 0;
            gDiagCbSum = gDiagEngSum = 0;
            gDiagBlocks = 0;
            gLateBlocks = gNearBlocks = 0;
            if (++diagSeconds % 10u == 0) gDiagInput = (gDiagInput + 1) % kDiagInputs;
        }
#endif
        System::Delay(1);
    }
}

#endif
