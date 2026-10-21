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
enum Section { kControl, kInput, kSwell, kTank, kTwin, kEra, kOutput, kNumSections };

#if defined(IV_PROFILE_HOOKS)
// Set by the host (profile firmware) before audio starts; null = no timing.
inline void (*markHook)(int section) = nullptr;
inline void mark(Section s)
{
    if (markHook) markHook(int(s));
}
#else
inline void mark(Section) {}
#endif

} // namespace iv::prof
