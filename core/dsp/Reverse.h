#pragma once
// REVERSE, the classic reverse swell (SPEC §4.11, ADR 0015). The tank's
// output is recorded in segments of TIME length; while one segment records,
// the one before it plays backwards, so each stretch of reverb rises to a
// cut, then the next. The reversed wet is therefore TIME late (every
// real-time reverse reverb is); the dry is not delayed.
//
// One ring buffer per side, at least two segments long. At position p of a
// segment the read sits 2p + 2 samples back (read(1) is the sample just
// written; the previous segment's last sample is read(2)), so it walks
// backwards through the previous segment one sample per sample. Each
// segment fades in and out over kEdgeSeconds so the cuts don't click.
//
// PROTOTYPE (M4 notes, "choppy and disjointed"): setSmooth(true) plays two
// backwards readers half a segment apart, each faded in and out over its
// whole length (sin², so the two always sum to one): no cuts at all, each
// stretch still runs backwards. The owner heard it as still chunky, with a
// break after the first repeat; renders/reverse_options now also tries the
// swell to a cut and the input reversed into the tank (Engine::setReverseMode).
#include "dsp/Memory.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

class Reverse {
public:
    static constexpr float kMaxSegmentSeconds = 2.0f;
    static constexpr float kEdgeSeconds = 0.005f;

    static constexpr size_t memoryFloats(float sampleRate)
    {
        return 2 * nextPow2(size_t(2.0f * kMaxSegmentSeconds * sampleRate) + 8);
    }

    bool prepare(Arena& arena, float sampleRate)
    {
        sampleRate_ = sampleRate;
        edge_ = kEdgeSeconds * sampleRate;
        const size_t n = size_t(2.0f * kMaxSegmentSeconds * sampleRate) + 4;
        const bool ok = line_[0].init(arena, n) && line_[1].init(arena, n);
        reset();
        return ok;
    }

    void reset()
    {
        line_[0].clear();
        line_[1].clear();
        pos_ = 0;
        length_ = pending_;
        for (int g = 0; g < 2; ++g) {
            gLen_[g] = pending_;
            gPos_[g] = g == 0 ? 0u : pending_ / 2u;
        }
    }

    void setSmooth(bool on) { smooth_ = on; }

    // Takes effect at the next segment start, so a segment never changes
    // length halfway (TIME moves are heard one segment later).
    void setSegmentSeconds(float s)
    {
        if (s > kMaxSegmentSeconds) s = kMaxSegmentSeconds;
        if (s < 0.05f) s = 0.05f;
        pending_ = uint32_t(s * sampleRate_);
    }

    // Records (l, r) and returns the reversed signal in place.
    void process(float& l, float& r)
    {
        if (smooth_) {
            line_[0].write(l);
            line_[1].write(r);
            float ol = 0.0f, orr = 0.0f;
            for (int g = 0; g < 2; ++g) {
                if (gPos_[g] >= gLen_[g]) {
                    gPos_[g] = 0;
                    gLen_[g] = pending_;
                }
                const float sn = std::sin(3.14159265f * float(gPos_[g]) / float(gLen_[g]));
                const uint32_t back = 2u * gPos_[g] + 2u;
                ol += sn * sn * line_[0].read(back);
                orr += sn * sn * line_[1].read(back);
                ++gPos_[g];
            }
            l = ol;
            r = orr;
            return;
        }
        if (pos_ >= length_) {
            pos_ = 0;
            length_ = pending_;
        }
        line_[0].write(l);
        line_[1].write(r);
        const uint32_t back = 2u * pos_ + 2u;
        const float p = float(pos_), rest = float(length_ - pos_);
        float fade = p < edge_ ? p / edge_ : 1.0f;
        if (rest < edge_) fade = rest / edge_ < fade ? rest / edge_ : fade;
        l = fade * line_[0].read(back);
        r = fade * line_[1].read(back);
        ++pos_;
    }

private:
    float     sampleRate_ = 48000.0f, edge_ = 240.0f;
    DelayLine line_[2];
    uint32_t  pos_ = 0, length_ = 24000, pending_ = 24000;
    uint32_t  gPos_[2] = {0, 12000}, gLen_[2] = {24000, 24000};
    bool      smooth_ = false;
};

} // namespace iv::dsp
