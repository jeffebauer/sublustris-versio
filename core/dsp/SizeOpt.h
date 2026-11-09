#pragma once
// IV_SIZE_OPT: build a control-rate function for size (-Os) in the firmware,
// where flash is the limit (ADR 0011: 128 KB). The per-sample DSP keeps -O3
// (firmware/Makefile). Mark only code that runs once per control tick or at
// set-up, never per sample. Desktop builds (and Clang) ignore it.
#if defined(IV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define IV_SIZE_OPT __attribute__((optimize("Os")))
#else
#define IV_SIZE_OPT
#endif

// IV_CTL_OPT / IV_CTL_NOINLINE: v1.0.51's flash trims on the per-block control maths (the DECAY / WEAR /
// SIZE mappings in params/Mappings.h, Engine::decayRate, and the one shared out-of-line blockCoeff;
// 816 B). As IV_SIZE_OPT / IV_NOINLINE, unless IV_CTL_O3 (firmware/Makefile CTL_O3=1): then -O3 and
// inlined as before v1.0.51, for the CPU hunt's A / B on the module (28 Oct 2026). Desktop: no effect.
#if defined(IV_CTL_O3)
#define IV_CTL_OPT
#define IV_CTL_NOINLINE
#else
#define IV_CTL_OPT IV_SIZE_OPT
#define IV_CTL_NOINLINE IV_NOINLINE
#endif

// IV_NO_UNSWITCH: -O3 as everywhere else, without loop unswitching, for the
// Resilio Tank's per-sample block (its Tank::process). Unswitching copied its whole input
// loop (TONE's tilt, the low cut, the coil, the Clang) once more for the one
// loop-invariant branch in it (the Clang's ceiling): 1.5 KB of flash for a
// predictable branch per sample (ADR 0038 Decision, "Flash").
#if defined(IV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define IV_NO_UNSWITCH __attribute__((optimize("no-unswitch-loops")))
#else
#define IV_NO_UNSWITCH
#endif

// IV_LEAN_LOOPS (firmware trial, review 2 audit, 28 Oct 2026; firmware/Makefile LEAN=1): Engine::process without loop
// unswitching (it copied the tank-input loop for ERA's hiss / crackle test, -540 B) and Fdn::runBlock without loop
// splitting (it split the early taps' last loop at s < q and the filling taps' at s >= lag, -474 B). Control flow
// only: the same floating-point operations in the same order (no fusing, no reassociation), so the same output; the
// cost is a predictable branch a sample in those loops. Off by default until a CPU lap has timed it.
#if defined(IV_LEAN_LOOPS) && defined(IV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define IV_LEAN_PROCESS __attribute__((optimize("no-unswitch-loops")))
#define IV_LEAN_RUNBLOCK __attribute__((optimize("no-unswitch-loops", "no-split-loops")))
#else
#define IV_LEAN_PROCESS
#define IV_LEAN_RUNBLOCK IV_NO_UNSWITCH
#endif

// IV_INLINE: always inline a small per-sample helper, even where the
// caller has grown past the compiler's inlining budget (the mu-law box's
// half-band steps inside Tank::process: called out of line, the two
// channels' filter chains can't overlap). IV_NOINLINE: one shared copy of a
// bigger one called from several places (flash).
#if defined(__GNUC__)
#define IV_INLINE inline __attribute__((always_inline))
#define IV_NOINLINE __attribute__((noinline))
#else
#define IV_INLINE inline
#define IV_NOINLINE
#endif

// IV_ZI: a member's zero initialiser ("float x_ IV_ZI;" is "float x_ = {};"), left out in the firmware.
// The firmware builds its Engine by value-initialisation (`new (gEngineStorage) iv::Engine()` in
// firmware/main.cpp; Engine has no user-provided constructor), which zeroes the whole object before
// the constructor runs, so storing those zeros again only cost flash: ~67 memsets and a few hundred
// stores in Engine() and Fdn() (overnight review 26 Oct 2026, section 5.6). Desktop hosts declare
// `iv::Engine e;` (default-initialisation, no zeroing) and keep the initialisers. Use it only on members
// of classes the firmware creates as part of the value-initialised Engine, by `new (p) T()` or with
// static storage (zeroed too: the profile build's micro-tests), never on a class a function builds as
// a local with `T x;` (DelayLine: Fdn::runBlock's `was`; EraMix, DriftAmounts, Absorb, Arena).
#if defined(IV_FIXED_VOICINGS)
#define IV_ZI
#else
#define IV_ZI = {}
#endif

// The SPACE morph's once-a-change code (ADR 0054) stays in flash, out of the 63 KB of ITCM it would otherwise overflow (the link script's .text
// takes .flash_text). Desktop: nothing.
#if defined(IV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define IV_MORPH_COLD __attribute__((section(".flash_text"), optimize("Os")))
#else
#define IV_MORPH_COLD
#endif
// IV_RI(v): a member's non-zero initialiser ("float x_ IV_RI(1.0f);"), left out where prepare() / reset() sets the same value
// before the first process(): the release firmware builds its Engine once by value-initialisation (see IV_ZI) and calls
// prepare() before anything else, so those stores only cost flash. Desktop hosts and the profile build (its micro-tests build
// Fdns and Swells of their own, without an Engine) keep them. A member prepare() / reset() doesn't set keeps its "= v"; proved
// by comparing the whole prepared Engine's bytes with and without (the flash hunt, Nov 2026).
#if defined(IV_FIXED_VOICINGS) && !defined(IV_MODE_PROFILE)
#define IV_RI(v)
#else
#define IV_RI(v) = v
#endif

// IV_D2_BSS: a Core array the firmware places in its D2 SRAM (the linker script's .d2_bss, NOLOAD: whoever uses it
// zeroes it). Only with IV_FIRMWARE_D2 (firmware/Makefile); elsewhere an ordinary array.
#if defined(IV_FIRMWARE_D2)
#define IV_D2_BSS __attribute__((section(".d2_bss")))
#else
#define IV_D2_BSS
#endif
