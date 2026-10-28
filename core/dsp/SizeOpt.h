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

// IV_D2_BSS: a Core array the firmware places in its D2 SRAM (the linker script's .d2_bss, NOLOAD: whoever uses it
// zeroes it). Only with IV_FIRMWARE_D2 (firmware/Makefile); elsewhere an ordinary array.
#if defined(IV_FIRMWARE_D2)
#define IV_D2_BSS __attribute__((section(".d2_bss")))
#else
#define IV_D2_BSS
#endif
