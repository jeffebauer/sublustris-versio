#pragma once
// Delay memory for the Core. The Core never allocates on the module: the
// firmware hands the Engine two plain float arrays, SDRAM for the long lines
// and DTCM for the small hot ones (SPEC §5.2), and each block takes what it
// needs from them at prepare(). Desktop Hosts let the Engine own vectors.
#include "dsp/SizeOpt.h"

#include <cstddef>
#include <cstdint>

namespace iv::dsp {

// A bump allocator over one float array. take() hands out zeroed slices
// in order; prepare() starts over with reset().
struct Arena {
    // Cache-set staggering for the SDRAM arena (M3 round 2, 25 Oct 2026). The STM32H750's data cache
    // is 16 KB, 4-way, 32-byte lines: 128 sets, picked by address bits 5-11. The power-of-two lines,
    // laid end to end, all started on the same set, and they all write at the same position each
    // sample, so every write of every line fought for the same 4 places and went out to SDRAM. Each
    // slice now starts on its own set (kSkewStep apart), at most kSkewLines lines of padding a slice.
    static constexpr size_t kSkewLines = 128, kLineFloats = 8, kSkewStep = 37;
    static constexpr size_t kMaxSlices = 256; // the room largeFloats() adds for the padding
    static constexpr size_t skewRoom() { return kMaxSlices * kSkewLines * kLineFloats; }

    float* base = nullptr;
    size_t size = 0, used = 0, slices = 0;
    bool   skew = false;

    void reset(float* b, size_t n, bool staggered = false)
    {
        base = b;
        size = n;
        used = 0;
        slices = 0;
        skew = staggered;
    }
    // nullptr when the arena is too small (the Engine then stays silent
    // rather than writing out of bounds).
    float* take(size_t n)
    {
        if (skew && base) {
            const size_t line = (reinterpret_cast<uintptr_t>(base + used) / (kLineFloats * sizeof(float))) % kSkewLines;
            const size_t want = (slices++ * kSkewStep) % kSkewLines;
            used += ((want + kSkewLines - line) % kSkewLines) * kLineFloats;
        }
        if (!base || used + n > size) return nullptr;
        float* p = base + used;
        used += n;
        for (size_t i = 0; i < n; ++i) p[i] = 0.0f;
        return p;
    }
};

constexpr size_t nextPow2(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// A power-of-two ring buffer: write one sample per tick, read any delay
// behind the write head. Integer reads, linear and cubic interpolation.
struct DelayLine {
    float*   buf  = nullptr;
    uint32_t mask = 0, pos = 0, fill = 0; // fill: writes since the last clear(), up to the length

    // Takes room for at least maxDelay + 4 samples (cubic needs 2 either side).
    bool init(Arena& arena, size_t maxDelaySamples)
    {
        const size_t n = nextPow2(maxDelaySamples + 4);
        buf = arena.take(n);
        mask = uint32_t(n - 1);
        pos = 0;
        fill = mask + 1; // handed out zeroed by the arena
        return buf != nullptr;
    }
    // Lazy (see above): the line forgets what it holds, no memory is touched.
    void clear() { fill = 0; }
    bool full() const { return fill > mask; }
    // The sample written a ticks ago (a = 1: the last write), silence if that was before a clear().
    float ago(uint32_t a) const { return a <= fill ? buf[(pos - a) & mask] : 0.0f; }
    void write(float x)
    {
        buf[pos] = x;
        pos = (pos + 1) & mask;
        if (fill <= mask) ++fill;
    }
    // The line as it will stand s writes from now (its head and fill moved on, nothing written): the
    // line-major tank (Fdn::runBlock) reads sample s of a block through it, exactly where the
    // sample-by-sample loop read.
    DelayLine ahead(uint32_t s) const
    {
        DelayLine v = *this;
        v.pos = (pos + s) & mask;
        v.fill = fill + s > mask ? mask + 1 : fill + s;
        return v;
    }
    // d >= 1: the sample written d ticks ago (d = 1 is the last write).
    float read(uint32_t d) const { return full() ? buf[(pos - d) & mask] : ago(d); }
    // Catmull-Rom cubic (Hermite) read at a fractional delay d >= 2.
    float readCubic(float d) const
    {
        if (!full()) return freshCubic(d);
        const float    fl = float(int32_t(d));
        const uint32_t i  = pos - uint32_t(int32_t(d));
        const float    t  = d - fl; // 0..1, toward older samples
        const float x0 = buf[(i + 1) & mask], x1 = buf[i & mask], x2 = buf[(i - 1) & mask],
                    x3 = buf[(i - 2) & mask];
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * t + c2) * t + c1) * t + x1;
    }
    // Allpass-interpolated read at a fractional delay d >= 2 (Dattorro 1997,
    // §"Interpolation"): magnitude 1 at every frequency, so a loop that reads
    // through it loses nothing however many passes a frozen pad makes (a
    // cubic read shaves the highs a little on every pass). `state` is this
    // read's own memory (the previous output). For slowly moving delays.
    //
    // The fraction is kept in [0.5, 1.5): near 0 the coefficient nears 1,
    // its pole sits by Nyquist, and a moving read excites it: a frozen pad
    // leaked energy into the top octave (MASSIVE 5.5 % → 11.7 % above 12 kHz
    // in 5 min). d >= 2.
    float readAllpass(float d, float& state) const
    {
        if (!full()) return freshAllpass(d, state);
        const auto  di  = uint32_t(int32_t(d - 0.5f));
        const float f   = d - float(di);
        const float eta = (1.0f - f) / (1.0f + f);
        const float a = buf[(pos - di) & mask], b = buf[(pos - di - 1) & mask];
        state = b + eta * (a - state);
        return state;
    }

private:
    // The same reads in the first trip after a clear(), through ago(): out of line, so every read
    // site carries only the compare (inlined, the copies cost ~1.9 KB of flash).
    IV_NOINLINE float freshCubic(float d) const
    {
        const uint32_t di = uint32_t(int32_t(d));
        const float    t  = d - float(int32_t(d));
        const float x0 = ago(di - 1), x1 = ago(di), x2 = ago(di + 1), x3 = ago(di + 2);
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * t + c2) * t + c1) * t + x1;
    }
    IV_NOINLINE float freshAllpass(float d, float& state) const
    {
        const auto  di  = uint32_t(int32_t(d - 0.5f));
        const float f   = d - float(di);
        const float eta = (1.0f - f) / (1.0f + f);
        state = ago(di + 1) + eta * (ago(di) - state);
        return state;
    }
};

// A ring buffer of exactly the length asked for (no power-of-two rounding),
// for memory-tight places (the Plate and its twin, SPEC §5.2): the
// wrap costs a compare instead of a mask.
struct ExactLine {
    float*   buf  = nullptr;
    uint32_t size = 0, pos = 0, fill = 0; // fill: as DelayLine's (clear() is lazy)

    bool init(Arena& arena, size_t maxDelaySamples)
    {
        size = uint32_t(maxDelaySamples + 4);
        buf = arena.take(size);
        pos = 0;
        fill = size; // handed out zeroed by the arena
        return buf != nullptr;
    }
    void clear() { fill = 0; }
    void write(float x)
    {
        buf[pos] = x;
        if (++pos == size) pos = 0;
        if (fill < size) ++fill;
    }
    float at(int32_t i) const // i: pos minus how long ago, any value within ±size of [0, size)
    {
        if (fill < size && int32_t(pos) - i > int32_t(fill)) return 0.0f; // written before a clear()
        if (i < 0) i += int32_t(size);
        else if (i >= int32_t(size)) i -= int32_t(size);
        return buf[i];
    }
    // Every sample of the ring written since the last clear(): its steady state, where the reads below
    // that end in Full skip at()'s fill test (it never fails) and wrap with one compare. A full ring
    // stays full until the next clear(), so a caller can ask once a block.
    bool full() const { return fill >= size; }
    // d >= 1: the sample written d ticks ago.
    float read(uint32_t d) const { return at(int32_t(pos) - int32_t(d)); }
    // Linear read between di and di + 1 ticks ago (di >= 1, t: the fraction toward the older one) of a
    // full ring, for a delay fixed through a block and split once (the Plate's output taps): the same
    // sums as at(i) + t * (at(i - 1) - at(i)).
    float lerpFull(int32_t di, float t) const
    {
        int32_t i = int32_t(pos) - di; // in (pos - size, pos - 1]
        if (i < 0) i += int32_t(size);
        const int32_t j = i == 0 ? int32_t(size) - 1 : i - 1;
        const float a = buf[i];
        return a + t * (buf[j] - a);
    }
    // Catmull-Rom cubic read at a fractional delay d >= 2.
    float readCubic(float d) const
    {
        const int32_t di = int32_t(d);
        const float   t  = d - float(di);
        const int32_t i  = int32_t(pos) - di;
        const float x0 = at(i + 1), x1 = at(i), x2 = at(i - 1), x3 = at(i - 2);
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * t + c2) * t + c1) * t + x1;
    }
    // As DelayLine::readAllpass.
    float readAllpass(float d, float& state) const
    {
        const int32_t di  = int32_t(d - 0.5f);
        const float   f   = d - float(di);
        const float   eta = (1.0f - f) / (1.0f + f);
        const int32_t i   = int32_t(pos) - di;
        state = at(i - 1) + eta * (at(i) - state);
        return state;
    }
    // The same for a full ring (the same sums, so the same bits); and at the whole samples and the
    // coefficient worked out once, for a delay fixed through a block.
    float readAllpassFull(float d, float& state) const
    {
        const int32_t di  = int32_t(d - 0.5f);
        const float   f   = d - float(di);
        return allpassFull(di, (1.0f - f) / (1.0f + f), state);
    }
    float allpassFull(int32_t di, float eta, float& state) const
    {
        int32_t i = int32_t(pos) - di; // in (pos - size, pos - 1]
        if (i < 0) i += int32_t(size);
        const int32_t j = i == 0 ? int32_t(size) - 1 : i - 1;
        state = buf[j] + eta * (buf[i] - state);
        return state;
    }
};

} // namespace iv::dsp
