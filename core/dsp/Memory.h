#pragma once
// Delay memory for the Core. The Core never allocates on the module: the
// firmware hands the Engine two plain float arrays, SDRAM for the long lines
// and DTCM for the small hot ones (SPEC §5.2), and each block takes what it
// needs from them at prepare(). Desktop Hosts let the Engine own vectors.
#include <cstddef>
#include <cstdint>

namespace iv::dsp {

// A bump allocator over one float array. take() hands out zeroed slices
// in order; prepare() starts over with reset().
struct Arena {
    float* base = nullptr;
    size_t size = 0, used = 0;

    void reset(float* b, size_t n)
    {
        base = b;
        size = n;
        used = 0;
    }
    // nullptr when the arena is too small (the Engine then stays silent
    // rather than writing out of bounds).
    float* take(size_t n)
    {
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
    uint32_t mask = 0, pos = 0;

    // Takes room for at least maxDelay + 4 samples (cubic needs 2 either side).
    bool init(Arena& arena, size_t maxDelaySamples)
    {
        const size_t n = nextPow2(maxDelaySamples + 4);
        buf = arena.take(n);
        mask = uint32_t(n - 1);
        pos = 0;
        return buf != nullptr;
    }
    void clear()
    {
        if (buf)
            for (uint32_t i = 0; i <= mask; ++i) buf[i] = 0.0f;
    }
    void write(float x)
    {
        buf[pos] = x;
        pos = (pos + 1) & mask;
    }
    // d >= 1: the sample written d ticks ago (d = 1 is the last write).
    float read(uint32_t d) const { return buf[(pos - d) & mask]; }
    // Catmull-Rom cubic (Hermite) read at a fractional delay d >= 2.
    float readCubic(float d) const
    {
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
    float readAllpass(float d, float& state) const
    {
        const auto  di  = uint32_t(int32_t(d));
        const float f   = d - float(di);
        const float eta = (1.0f - f) / (1.0f + f);
        const float a = buf[(pos - di) & mask], b = buf[(pos - di - 1) & mask];
        state = b + eta * (a - state);
        return state;
    }
};

// A ring buffer of exactly the length asked for (no power-of-two rounding),
// for memory-tight places (the Plate and its companion, SPEC §5.2): the
// wrap costs a compare instead of a mask.
struct ExactLine {
    float*   buf  = nullptr;
    uint32_t size = 0, pos = 0;

    bool init(Arena& arena, size_t maxDelaySamples)
    {
        size = uint32_t(maxDelaySamples + 4);
        buf = arena.take(size);
        pos = 0;
        return buf != nullptr;
    }
    void clear()
    {
        if (buf)
            for (uint32_t i = 0; i < size; ++i) buf[i] = 0.0f;
    }
    void write(float x)
    {
        buf[pos] = x;
        if (++pos == size) pos = 0;
    }
    float at(int32_t i) const // i: index, any value within ±size of [0, size)
    {
        if (i < 0) i += int32_t(size);
        else if (i >= int32_t(size)) i -= int32_t(size);
        return buf[i];
    }
    // d >= 1: the sample written d ticks ago.
    float read(uint32_t d) const { return at(int32_t(pos) - int32_t(d)); }
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
        const int32_t di  = int32_t(d);
        const float   f   = d - float(di);
        const float   eta = (1.0f - f) / (1.0f + f);
        const int32_t i   = int32_t(pos) - di;
        state = at(i - 1) + eta * (at(i) - state);
        return state;
    }
};

} // namespace iv::dsp
