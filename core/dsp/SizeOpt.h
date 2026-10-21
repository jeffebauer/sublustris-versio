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
