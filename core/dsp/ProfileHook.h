#pragma once

// Optional per-section timing for the M3 profile firmware. Host-neutral: the
// Core only calls a function pointer that a host may set. Compiled out unless
// IV_PROFILE_HOOKS is defined (the profile firmware only), so the release
// firmware, the Plugin and the Renderer pay nothing.
//
// mark(s) means "section s just ended": the host charges the time since the
// previous mark to s.

namespace iv::prof {

// Immensio's sections, in signal order (SPEC §4.1, §5.1).
// kTwin: the twin's input side (diffusers, swell) and the twin tank; kShimmer: its recording and heads.
// Inside the tanks (M3 round 4): kTankCtl, each tank's once-a-block maths; kTankLoop, the FDN's lines
// (read, in-loop allpasses, loss filter, ERA's in-loop stages); kTankOut, its output and early taps;
// kTankMix, the Hadamard mix and the writes. kTank keeps the rest (the Plate's loop, the still-lows split).
// kEraWear: ERA's wear on the wet (v1.0.42, dsp/EraTexture.h; until then kEraTape, kEraBbd, kEraRadio).
enum Section { kControl, kInput, kShimmer, kSwell, kTwin, kTank, kTankCtl, kTankLoop, kTankOut, kTankMix, kEraWear, kOutput, kNumSections };

#if defined(IV_PROFILE_HOOKS)
// Set by the host (profile firmware) before audio starts; null = no timing.
inline void (*markHook)(int section) = nullptr;
inline void mark(Section s)
{
    if (markHook) markHook(int(s));
}
// SIZE crossfade counters (the size_moves hunt, 26 Oct 2026), read and cleared by the host each block:
// crossfades started, line-blocks that read twice (a line crossfading over one control block), and how
// many tanks (FDNs: the tank, the still lows, the twin) crossfaded in the block.
inline int         xfStarts = 0, xfLineBlocks = 0, xfTanks = 0;
inline const void* xfTank[4] = {};
inline void countXfStart() { ++xfStarts; }
inline void countXfLines(const void* tank)
{
    ++xfLineBlocks;
    for (int k = 0; k < xfTanks; ++k)
        if (xfTank[k] == tank) return;
    if (xfTanks < 4) xfTank[xfTanks++] = tank;
}
// The SIZE-spike lap's A / B (27 Oct 2026): false runs SIZE's crossfade on v1.0.48's paths (Fdn::lineSlow and
// Fdn::tapsOld for every crossfading line), true on the faster ones (Fdn::lineXf, the carried old taps), which give
// the same output to the bit. Builds without the hooks always take the faster ones.
inline bool xfFast = true;
#else
inline void mark(Section) {}
inline void countXfStart() {}
inline void countXfLines(const void*) {}
#endif

} // namespace iv::prof
