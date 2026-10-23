#include "Metrics.h"
#include "Fft.h"
#include "Spectral.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace iv::metrics {

namespace {

// Event = sample exceeding -40 dBFS after >= 0.5 s continuously under it
// (docs/m1-contracts.md). Returns event onset sample indices.
std::vector<size_t> findEvents(const std::vector<float>& mono, float sr)
{
    const float thresh = spectral::fromDb(-40.0f);
    const size_t armSamples = size_t(0.5 * double(sr));
    std::vector<size_t> events;
    size_t under = armSamples; // treat the start as already "armed"
    size_t i = 0;
    while (i < mono.size()) {
        if (std::fabs(mono[i]) >= thresh) {
            if (under >= armSamples) events.push_back(i);
            while (i < mono.size() && std::fabs(mono[i]) >= thresh) ++i;
            under = 0;
        } else {
            ++under;
            ++i;
        }
    }
    return events;
}

// Mean power of 10 ms blocks over [start, end) (a trailing partial block
// is dropped).
std::vector<double> blockPowers(const std::vector<float>& mono, size_t start, size_t end, size_t block)
{
    const size_t numBlocks = end > start ? (end - start) / block : 0;
    std::vector<double> e(numBlocks, 0.0);
    for (size_t b = 0; b < numBlocks; ++b) {
        const size_t from = start + b * block;
        for (size_t i = from; i < from + block; ++i) e[b] += double(mono[i]) * double(mono[i]);
        e[b] /= double(block);
    }
    return e;
}

// Below this (re the loudest 10 ms block) a segment's floor is too low to
// matter: the T60 fit runs exactly as on a noise-free render.
constexpr double kT60FloorNegligibleDb = -90.0;
// Digital silence (a gap of zeros, a render's tail below float precision)
// isn't a noise floor: blocks this far below the peak are left out of the
// floor estimate.
constexpr double kT60SilenceDb = -120.0;
// With a floor: integrate until the 50 ms envelope falls to this far above
// it, and let the fit end (-35 dB) no closer to it than this. 7 dB: noisy
// synthetic tails of known T60 (1 s, 3.5 s) read within 1 % whenever they
// pass; the owner's Wellspring take A (floor ~52 dB below its peak) passes
// at 3.42 s (combined-click 3.52 s), its noisier A-L / A-R read null.
constexpr double kT60CutAboveFloorDb = 3.0;
constexpr double kT60ClearAboveFloorDb = 7.0;

// Noise floor of a recording, for the T60 fit: the mean power of the
// quietest 200 ms after the segment's loudest 10 ms block, over [start,
// end) (the whole segment, up to the next event's -40 dBFS crossing).
// Windows touching digital silence don't count, so a render (or a gap of
// zeros) gives 0: no floor. Averaging 200 ms keeps the estimate steady; it
// can only read high by including some tail, which errs toward cutting
// the fit sooner (see schroederT60).
double noiseFloorPower(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    const size_t block = std::max<size_t>(1, size_t(0.010 * double(sr)));
    if (end > mono.size() || end <= start + block) return 0.0;
    const std::vector<double> e = blockPowers(mono, start, end, block);
    const size_t peak = size_t(std::max_element(e.begin(), e.end()) - e.begin());
    const double silent = e[peak] * std::pow(10.0, kT60SilenceDb / 10.0);
    constexpr size_t kWin = 20; // 200 ms
    double best = 0.0;
    bool found = false;
    double sum = 0.0;
    size_t live = 0; // consecutive non-silent blocks ending at b
    for (size_t b = peak + 1; b < e.size(); ++b) {
        if (e[b] <= silent) { live = 0; sum = 0.0; continue; }
        sum += e[b];
        if (++live > kWin) { sum -= e[b - kWin]; live = kWin; }
        if (live == kWin && (!found || sum / double(kWin) < best)) { best = sum / double(kWin); found = true; }
    }
    return found ? best : 0.0;
}

// Schroeder backward-integration T60: fit the -5 -> -35 dB region of the
// energy decay curve, extrapolate the slope to -60 dB. NaN if the curve
// never reaches -35 dB in [start, end) (not measurable).
//
// `floorPower` is the recording's noise floor (noiseFloorPower above; 0
// for none). Integrated as-is, a floor's energy props up the decay curve
// and the tail reads far too long (a Wellspring take ~50 dB above its hiss
// read 14 s against a true ~3.5 s). So, when the floor is within 90 dB of
// the peak (Lundeby-style, like tools/ingest_references.py's combined-click
// fit):
//  - the integration stops where the 50 ms envelope falls to 3 dB above
//    the floor, and the floor's power is subtracted from every sample
//    before it;
//  - the tail energy lost past that point is added back, assuming it keeps
//    decaying at the fitted rate (a few rounds converge);
//  - the fit must end (at -35 dB) while the envelope is still >= 7 dB
//    above the floor. Otherwise the tail never rises clearly enough above
//    the noise to measure (a floor less than ~42 dB below the tail's
//    start): NaN, not a guess.
// With no floor (digital silence, or one more than 90 dB down) this is the
// plain fit, unchanged.
double schroederT60(const std::vector<float>& mono, size_t start, size_t end, float sr, double floorPower)
{
    if (end <= start + 8 || end > mono.size()) return std::nan("");
    size_t len = end - start;

    // Noise-floor handling: where to stop integrating (len), the power left
    // in the tail there (tailPower), and how far the fit may go (clearEnd).
    double nf = 0.0, tailPower = 0.0;
    size_t clearEnd = len;
    const size_t block = std::max<size_t>(1, size_t(0.010 * double(sr)));
    if (floorPower > 0.0 && len >= 8 * block) {
        const std::vector<double> e = blockPowers(mono, start, end, block);
        const size_t peak = size_t(std::max_element(e.begin(), e.end()) - e.begin());
        if (floorPower > e[peak] * std::pow(10.0, kT60FloorNegligibleDb / 10.0)) {
            nf = floorPower;
            // 50 ms envelope: centred mean of 5 blocks.
            auto env = [&](size_t b) {
                const size_t lo = b >= 2 ? b - 2 : 0, hi = std::min(e.size(), b + 3);
                double s = 0.0;
                for (size_t k = lo; k < hi; ++k) s += e[k];
                return s / double(hi - lo);
            };
            const double cutLevel = nf * std::pow(10.0, kT60CutAboveFloorDb / 10.0);
            const double clearLevel = nf * std::pow(10.0, kT60ClearAboveFloorDb / 10.0);
            size_t cut = e.size(), clear = e.size();
            for (size_t b = peak; b < e.size(); ++b) {
                const double v = env(b);
                if (clear == e.size() && v <= clearLevel) clear = b;
                if (v <= cutLevel) { cut = b; break; }
            }
            clearEnd = std::min(len, clear * block);
            if (cut < e.size()) len = cut * block;
            tailPower = std::max(env(std::min(cut, e.size() - 1)) - nf, 0.0);
        }
    }
    if (len <= 8) return std::nan("");

    std::vector<double> edc(len);
    double acc = 0.0;
    for (size_t n = len; n-- > 0;) {
        const double x = double(mono[start + n]);
        acc += x * x - nf;
        edc[n] = acc;
    }
    if (edc[0] <= 0.0) return std::nan("");

    double t60 = std::nan(""), extra = 0.0;
    for (int round = 0; round < (nf > 0.0 ? 6 : 1); ++round) {
        const double e0 = edc[0] + extra;
        auto dbAt = [&](size_t i) { return 10.0 * std::log10(std::max(edc[i] + extra, 1e-300) / e0); };
        long n5 = -1, n35 = -1;
        for (size_t i = 0; i < len; ++i) {
            const double db = dbAt(i);
            if (n5 < 0 && db <= -5.0) n5 = long(i);
            if (db <= -35.0) { n35 = long(i); break; }
        }
        if (n5 < 0 || n35 < 0 || n35 <= n5) return std::nan("");
        if (size_t(n35) >= clearEnd) return std::nan(""); // fit would reach into the noise

        double sumT = 0, sumD = 0, sumTT = 0, sumTD = 0;
        long count = 0;
        for (long i = n5; i <= n35; ++i) {
            const double t = double(i) / double(sr);
            const double db = dbAt(size_t(i));
            sumT += t; sumD += db; sumTT += t * t; sumTD += t * db; ++count;
        }
        const double denom = double(count) * sumTT - sumT * sumT;
        if (std::fabs(denom) < 1e-12) return std::nan("");
        const double slope = (double(count) * sumTD - sumT * sumD) / denom; // dB/s, expect negative
        if (slope >= -1e-6) return std::nan(""); // not decaying
        t60 = -60.0 / slope;
        // Tail past the cut: tailPower * exp(-t / tau), tau = T60 / (6 ln 10),
        // integrated in samples.
        extra = tailPower * double(sr) * t60 / (6.0 * std::log(10.0));
    }
    return t60;
}

// End of the T60 fit when another event follows at `end`. The next event
// doesn't start at its -40 dBFS crossing: a spring's output (or a fading-in
// stimulus) builds up quietly for tens of ms before it, and that energy,
// fed into the backward integration, props up the end of the decay curve
// and makes the tail read long (1.23 s vs 1.03 s on one render). So the
// fit stops at the quietest point between the tail's peak and the next
// onset: the lowest-energy 10 ms block after the loudest one (the latest
// on ties, e.g. digital silence). Before it the tail is still falling;
// after it the next event is arriving. Cutting the tail there costs
// nothing measurable: by then it is at its floor, far below the -35 dB
// end of the fit. Returns `end` if the segment is too short to split.
size_t decayEnd(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    const size_t block = std::max<size_t>(1, size_t(0.010 * double(sr)));
    if (end > mono.size() || end <= start + 4 * block) return end;
    const size_t numBlocks = (end - start) / block;
    std::vector<double> e(numBlocks, 0.0);
    for (size_t b = 0; b < numBlocks; ++b) {
        const size_t from = start + b * block;
        for (size_t i = from; i < from + block; ++i) e[b] += double(mono[i]) * double(mono[i]);
    }
    const size_t peak = size_t(std::max_element(e.begin(), e.end()) - e.begin());
    size_t quietest = peak;
    for (size_t b = peak; b < numBlocks; ++b)
        if (e[b] <= e[quietest]) quietest = b;
    if (quietest == peak) return end;
    return start + quietest * block;
}

// Average power spectrum over [start, end), Hann-windowed FFT (8192, or
// the largest power of two <= 8192 that fits the segment, min 256), then
// max over 100 Hz-10 kHz of (bin dB - 1/3-octave-smoothed median dB).
double resonancePeakDb(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    if (end <= start || end > mono.size()) return std::nan("");
    const size_t segLen = end - start;
    size_t fftSize = 8192;
    while (fftSize > 256 && fftSize > segLen) fftSize >>= 1;
    if (segLen < 32) return std::nan("");

    const size_t hop = fftSize / 2;
    std::vector<double> sumPower(fftSize / 2 + 1, 0.0);
    size_t frames = 0;
    for (size_t pos = start; pos + std::min(fftSize, segLen) <= end; pos += hop) {
        const size_t n = std::min(fftSize, end - pos);
        const auto mag = fft::magnitudeSpectrum(mono.data() + pos, n, fftSize);
        for (size_t k = 0; k < mag.size(); ++k) sumPower[k] += double(mag[k]) * double(mag[k]);
        ++frames;
        if (n < fftSize) break; // last partial frame
    }
    if (frames == 0) {
        const auto mag = fft::magnitudeSpectrum(mono.data() + start, segLen, fftSize);
        for (size_t k = 0; k < mag.size(); ++k) sumPower[k] += double(mag[k]) * double(mag[k]);
        frames = 1;
    }

    std::vector<float> magDb(sumPower.size()), freqHz(sumPower.size());
    for (size_t k = 0; k < sumPower.size(); ++k) {
        magDb[k] = spectral::toDb(float(std::sqrt(sumPower[k] / double(frames))));
        freqHz[k] = float(k) * sr / float(fftSize);
    }
    const auto smoothed = spectral::thirdOctaveSmoothedMedian(magDb, freqHz);

    float best = -1e9f;
    for (size_t k = 0; k < magDb.size(); ++k) {
        if (freqHz[k] < 100.0f || freqHz[k] > 10000.0f) continue;
        best = std::max(best, magDb[k] - smoothed[k]);
    }
    return best > -1e8f ? double(best) : std::nan("");
}

// True if a narrowband peak stays > 12 dB above the smoothed spectrum, at
// an unchanged bin (+-1), across consecutive 0.5 s frames covering > 2 s,
// while its estimated level is > -30 dBFS.
bool steadyTone(const std::vector<float>& mono, float sr)
{
    const size_t frameSamples = size_t(0.5 * double(sr));
    if (frameSamples < 64 || mono.size() < frameSamples) return false;

    // A fixed FFT size for the per-frame snapshot: the 1/3-octave
    // smoothing below is O(bins^2), so padding all the way out to match a
    // full 0.5 s frame (tens of thousands of bins) would make a 60 s file
    // take minutes. 8192 (~5.9 Hz/bin at 48 kHz) keeps that cost bounded
    // while still giving the 1/3-octave window enough bins even at low
    // frequencies to smooth around a narrowband peak rather than through
    // it; it analyzes a representative ~170 ms slice of each 0.5 s frame.
    const size_t fftSize = std::min<size_t>(8192, frameSamples);
    const size_t windowLen = std::min(frameSamples, fftSize);

    const size_t numFrames = mono.size() / frameSamples;
    // Every bin keeps its own run (M6 fix): the old version followed only
    // the single most prominent bin per frame, so a steady tone whose
    // harmonics are about as prominent (a saturated, self-oscillating loop)
    // flipped between them and never built a run.
    const size_t bins = fftSize / 2 + 1;
    std::vector<long> run(bins, 0), prev(bins, 0);
    long bestRun = 0;
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * frameSamples;
        const auto mag = fft::magnitudeSpectrum(mono.data() + start, windowLen, fftSize);
        std::vector<float> magDb(mag.size()), freqHz(mag.size());
        for (size_t k = 0; k < mag.size(); ++k) {
            magDb[k] = spectral::toDb(mag[k]);
            freqHz[k] = float(k) * sr / float(fftSize);
        }
        const auto smoothed = spectral::thirdOctaveSmoothedMean(magDb, freqHz);
        prev.swap(run);
        for (size_t k = 0; k < bins; ++k) {
            run[k] = 0;
            if (k == 0 || magDb[k] - smoothed[k] <= 12.0f) continue; // skip DC
            // Amplitude of an isolated Hann-windowed tone: |X[k]| ~= A*n/4.
            const double amp = 4.0 * double(mag[k]) / double(windowLen);
            if (spectral::toDb(float(amp)) <= -30.0) continue;
            long p = prev[k];
            if (k > 0) p = std::max(p, prev[k - 1]);
            if (k + 1 < bins) p = std::max(p, prev[k + 1]);
            run[k] = p + 1;
            bestRun = std::max(bestRun, run[k]);
        }
    }
    const double runSeconds = double(bestRun) * (double(frameSamples) / double(sr));
    return runSeconds > 2.0;
}

// Sample-to-sample discontinuities: |x[n]-2x[n-1]+x[n-2]| exceeding 20 dB
// above a local (+-10 ms) RMS of that second-difference signal, and above
// -60 dBFS absolute (docs/m1-contracts.md). Debounced 1 ms so one click
// isn't counted many times.
//
// Tuning note: a bare "20 dB above local RMS" test also fires on ordinary
// hard transients coming out of silence (e.g. 04_skank.wav's un-faded
// chord stabs) -- right at the onset the local RMS of the difference
// signal is itself near zero, so almost any attack clears the bar. A real
// click is a singular glitch: the raw signal returns to its prior level
// right after it. A musical onset instead settles into a new, sustained
// level. So a candidate only counts if the raw signal does NOT show a
// sustained level jump: RMS of the raw signal well before the candidate
// (200-20 ms back, clear of it) is compared against RMS just after it
// (5-50 ms forward, skipping the glitch's own few samples). A >5x jump
// means "new sustained sound", not a click. Verified against all six
// stimulus files (see final report) and a sine with an injected step.
long clickCount(const std::vector<float>& mono, float sr)
{
    const size_t n = mono.size();
    if (n < 3) return 0;
    std::vector<double> dsq(n, 0.0), xsq(n, 0.0);
    for (size_t i = 0; i < n; ++i) xsq[i] = double(mono[i]) * double(mono[i]);
    for (size_t i = 2; i < n; ++i) {
        const double d = double(mono[i]) - 2.0 * double(mono[i - 1]) + double(mono[i - 2]);
        dsq[i] = d * d;
    }
    std::vector<double> dPrefix(n + 1, 0.0), xPrefix(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) { dPrefix[i + 1] = dPrefix[i] + dsq[i]; xPrefix[i + 1] = xPrefix[i] + xsq[i]; }

    auto rmsX = [&](long lo, long hi) -> double {
        lo = std::max<long>(lo, 0);
        hi = std::min<long>(hi, long(n) - 1);
        if (hi < lo) return 0.0;
        const size_t cnt = size_t(hi - lo + 1);
        return std::sqrt((xPrefix[size_t(hi) + 1] - xPrefix[size_t(lo)]) / double(cnt));
    };

    const size_t halfWin = std::max<size_t>(1, size_t(0.010 * double(sr)));
    const double absFloor = spectral::fromDb(-60.0f);
    const size_t debounce = std::max<size_t>(1, size_t(0.001 * double(sr)));
    const long bgLo = long(0.200 * double(sr)), bgHi = long(0.020 * double(sr));
    const long trailLo = long(0.005 * double(sr)), trailHi = long(0.050 * double(sr));
    constexpr double kOnsetFactor = 5.0;

    long count = 0;
    size_t i = 2;
    while (i < n) {
        const size_t lo = i > halfWin ? i - halfWin : 0;
        const size_t hi = std::min(n - 1, i + halfWin);
        const double localRms = std::sqrt(dPrefix[hi + 1] - dPrefix[lo]) / std::sqrt(double(hi - lo + 1));
        const double threshold = std::max(localRms * 10.0, absFloor); // 20 dB above local RMS
        if (std::sqrt(dsq[i]) > threshold) {
            const double bg = rmsX(long(i) - bgLo, long(i) - bgHi);
            const double trail = rmsX(long(i) + trailLo, long(i) + trailHi);
            const bool sustainedOnset = trail > std::max(bg * kOnsetFactor, 1e-3);
            if (!sustainedOnset) {
                ++count;
                i += debounce;
                continue;
            }
        }
        ++i;
    }
    return count;
}

// Pearson correlation of L and R over [start, end) (docs/m4-contracts.md
// Stream E). NaN if the segment is empty/out of range or either channel
// has ~zero variance there (undefined correlation).
double pearsonCorrelation(const std::vector<float>& L, const std::vector<float>& R, size_t start, size_t end)
{
    if (end <= start || end > L.size() || end > R.size()) return std::nan("");
    const size_t n = end - start;
    double sumL = 0.0, sumR = 0.0;
    for (size_t i = start; i < end; ++i) { sumL += double(L[i]); sumR += double(R[i]); }
    const double meanL = sumL / double(n), meanR = sumR / double(n);
    double cov = 0.0, varL = 0.0, varR = 0.0;
    for (size_t i = start; i < end; ++i) {
        const double dl = double(L[i]) - meanL, dr = double(R[i]) - meanR;
        cov += dl * dr; varL += dl * dl; varR += dr * dr;
    }
    if (varL <= 1e-20 || varR <= 1e-20) return std::nan("");
    return cov / std::sqrt(varL * varR);
}

// 10*log10( power(L+R) / (power(L)+power(R)) ) over the whole file
// (docs/m4-contracts.md Stream E). Uncorrelated -> ~0 dB, identical ->
// +3 dB, out of phase -> very negative (floored so the result stays
// finite rather than -inf for exact cancellation).
double monoLossDb(const std::vector<float>& L, const std::vector<float>& R)
{
    const size_t n = std::min(L.size(), R.size());
    if (n == 0) return std::nan("");
    double sumMono = 0.0, sumSep = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double l = double(L[i]), r = double(R[i]);
        const double mixed = l + r;
        sumMono += mixed * mixed;
        sumSep += l * l + r * r;
    }
    if (sumSep <= 1e-300) return std::nan(""); // silence: undefined
    return 10.0 * std::log10(std::max(sumMono, 1e-300) / sumSep);
}

// Deepest dip in 200Hz-5kHz of the mono-sum power spectrum relative to the
// stereo-average power spectrum ((|L|^2+|R|^2)/2), both averaged over
// Hann-windowed frames and 1/3-octave-smoothed (docs/m4-contracts.md
// Stream E). Destructive interference between L and R shows up as a comb
// notch in the mono sum that the stereo-average (phase-blind) spectrum
// doesn't have, so the difference isolates comb-filtering, not just
// spectral shape. Same 8192-point-or-smaller-power-of-two framing as
// resonancePeakDb.
double monoNotchDb(const std::vector<float>& L, const std::vector<float>& R, size_t start, size_t end, float sr)
{
    if (end <= start || end > L.size() || end > R.size()) return std::nan("");
    const size_t segLen = end - start;
    size_t fftSize = 8192;
    while (fftSize > 256 && fftSize > segLen) fftSize >>= 1;
    if (segLen < 32) return std::nan("");

    std::vector<float> monoSum(segLen);
    for (size_t i = 0; i < segLen; ++i) monoSum[i] = L[start + i] + R[start + i];

    const size_t hop = fftSize / 2;
    std::vector<double> sumMonoPow(fftSize / 2 + 1, 0.0), sumAvgPow(fftSize / 2 + 1, 0.0);
    size_t frames = 0;
    for (size_t pos = 0; pos + std::min(fftSize, segLen) <= segLen; pos += hop) {
        const size_t n = std::min(fftSize, segLen - pos);
        const auto magM = fft::magnitudeSpectrum(monoSum.data() + pos, n, fftSize);
        const auto magL = fft::magnitudeSpectrum(L.data() + start + pos, n, fftSize);
        const auto magR = fft::magnitudeSpectrum(R.data() + start + pos, n, fftSize);
        for (size_t k = 0; k < magM.size(); ++k) {
            sumMonoPow[k] += double(magM[k]) * double(magM[k]);
            sumAvgPow[k] += 0.5 * (double(magL[k]) * double(magL[k]) + double(magR[k]) * double(magR[k]));
        }
        ++frames;
        if (n < fftSize) break;
    }
    if (frames == 0) {
        const auto magM = fft::magnitudeSpectrum(monoSum.data(), segLen, fftSize);
        const auto magL = fft::magnitudeSpectrum(L.data() + start, segLen, fftSize);
        const auto magR = fft::magnitudeSpectrum(R.data() + start, segLen, fftSize);
        for (size_t k = 0; k < magM.size(); ++k) {
            sumMonoPow[k] += double(magM[k]) * double(magM[k]);
            sumAvgPow[k] += 0.5 * (double(magL[k]) * double(magL[k]) + double(magR[k]) * double(magR[k]));
        }
        frames = 1;
    }

    std::vector<float> monoDb(sumMonoPow.size()), avgDb(sumMonoPow.size()), freqHz(sumMonoPow.size());
    for (size_t k = 0; k < sumMonoPow.size(); ++k) {
        monoDb[k] = spectral::toDb(float(std::sqrt(sumMonoPow[k] / double(frames))));
        avgDb[k]  = spectral::toDb(float(std::sqrt(sumAvgPow[k] / double(frames))));
        freqHz[k] = float(k) * sr / float(fftSize);
    }
    const auto monoSmoothed = spectral::thirdOctaveSmoothedMedian(monoDb, freqHz);
    const auto avgSmoothed  = spectral::thirdOctaveSmoothedMedian(avgDb, freqHz);

    float worst = 1e9f;
    for (size_t k = 0; k < monoSmoothed.size(); ++k) {
        if (freqHz[k] < 200.0f || freqHz[k] > 5000.0f) continue;
        worst = std::min(worst, monoSmoothed[k] - avgSmoothed[k]);
    }
    return worst < 1e8f ? double(worst) : std::nan("");
}

// Largest absolute change in RMS dB between consecutive, non-overlapping
// 100 ms windows, ignoring windows below -60 dBFS and the first 100 ms
// window after each event onset (docs/m4-contracts.md Stream E; event =
// as in t60, see findEvents above). Runs on the mono downmix regardless
// of channel count, like t60/resonance.
double maxStepDb100ms(const std::vector<float>& mono, float sr, const std::vector<size_t>& events)
{
    const size_t winLen = size_t(0.1 * double(sr));
    if (winLen == 0 || mono.size() < winLen) return std::nan("");
    const size_t numWindows = mono.size() / winLen;
    if (numWindows < 2) return std::nan("");

    std::vector<double> winDb(numWindows);
    std::vector<bool> eligible(numWindows, true);
    for (size_t w = 0; w < numWindows; ++w) {
        const size_t start = w * winLen, end = start + winLen;
        double sumSq = 0.0;
        for (size_t i = start; i < end; ++i) sumSq += double(mono[i]) * double(mono[i]);
        const double rms = std::sqrt(sumSq / double(winLen));
        winDb[w] = rms > 0.0 ? double(spectral::toDb(float(rms))) : -200.0;
        if (winDb[w] < -60.0) eligible[w] = false;
    }
    for (size_t onset : events) {
        const size_t w = onset / winLen;
        if (w < numWindows) eligible[w] = false;
    }

    double maxStep = 0.0;
    bool any = false;
    for (size_t w = 0; w + 1 < numWindows; ++w) {
        if (!eligible[w] || !eligible[w + 1]) continue;
        maxStep = std::max(maxStep, std::fabs(winDb[w + 1] - winDb[w]));
        any = true;
    }
    return any ? maxStep : std::nan("");
}

// ---- M6 Ringing metric (docs/m6-metric-calibration.md) ----------------------
//
// Plain version: a Ringing tone is one narrow frequency that outlives the
// frequencies around it. So instead of asking "is there a peak?" (a spring's
// own mode comb is full of peaks, which is why resonance_peak_db reads
// 10-40 dB on real tanks), ask "does a peak keep *pulling away from* its
// neighbourhood as the tail dies away?".
//
// 1. STFT of the segment (4-term Blackman-Harris, ~170 ms frames = 8192 at
//    48 kHz, hop 1/4). Power pooled over 3 bins and averaged over
//    kRingingSmoothS in time (evens out beating), in dB.
// 2. Tail start: after the frame where the median bin level (100 Hz-10 kHz;
//    a median ignores any single tone) peaks, plus one frame length and the
//    smoothing half-width, so the input has stopped and only the tail is
//    measured. The input's own tonal content (a snare's 185 Hz body) starts
//    loud but then decays at the tank's rate like everything else, so it
//    does not "grow".
// 3. At up to 96 frames over the rest of the segment: each bin's
//    neighbourhood level = median of the bins within 1/3 octave (at least
//    +-10 bins), leaving out the bin's own +-4; prominence = bin - that.
// 4. Growth, per bin: over the frames where the neighbourhood is still a
//    real reference (within kRingingSpanDb of its own start and of the
//    tail's typical start level, 15 dB above its own floor, peak not yet
//    kRingingLeakDb clear), take the late half, and
//    fit a robust (Theil-Sen) line to prominence vs time. Growth = dB gained
//    over that late half. Counted only for narrow peaks (louder than +-3
//    bins; a bin's level follows its peak over +-kRingingFollowBins, so a
//    mode drifting with WOBBLE stays one peak) that end >=
//    kRingingEndProminenceDb clear, climb steadily (the late half's thirds
//    rise in order, each step by kRingingSteadyShare of the rise: a bump or
//    one early jump is not growth) and last (decay
//    no faster than a T60 of kRingingMinT60Ratio x the tail's own T60). The late half only:
//    real tanks have an early phase where their mode peaks emerge from the
//    initial burst (the valleys drain first), then every mode decays
//    together; a Ringing mode keeps pulling away.
// 5. Steady tone, per bin: Ringing's end state (a self-oscillating loop, or
//    a mode so slow it left its neighbourhood behind and stopped "growing"):
//    >= kRingingSteadyProminenceDb clear, within kRingingSteadyRangeDb of
//    the tail's start level, decaying slower than kRingingSteadyDbPerS for
//    >= kRingingSteadyS. Score = its median prominence (capped at
//    kRingingLeakDb, "a bare tone").
// ringingDb = the largest score over 100 Hz-10 kHz; Ringing if >= kRingingGrowthDb.
struct RingingResult {
    double db = std::nan(""), ratio = std::nan(""), hz = std::nan(""), endDb = std::nan(""), spanS = std::nan("");
};

RingingResult ringingGrowth(const std::vector<float>& mono, size_t segStart, size_t segEnd, float sr, double tailT60)
{
    RingingResult r;
    size_t N = 256;
    while (double(N) * 1.5 < 0.171 * double(sr)) N <<= 1; // 8192 at 44.1/48 kHz, 16384 at 96 kHz
    const size_t hop = N / 4;
    const double hopS = double(hop) / double(sr);
    const size_t halfW = std::max<size_t>(1, size_t(std::lround(kRingingSmoothS / 2.0 / hopS)));
    if (segEnd > mono.size() || segEnd <= segStart || segEnd - segStart < N + (4 * halfW + 12) * hop) return r;
    const size_t F = (segEnd - segStart - N) / hop + 1;

    const double binHz = double(sr) / double(N);
    constexpr size_t kMinHalf = 10, kGuard = 4;
    const double kThirdHalf = std::pow(2.0, 1.0 / 6.0);
    const size_t kLo = std::max<size_t>(2, size_t(std::ceil(100.0 / binHz)));
    const size_t kHi = std::min(N / 2 - 2, size_t(std::floor(std::min(10000.0, 0.45 * double(sr)) / binHz)));
    if (kHi <= kLo + 2 * kMinHalf) return r;
    const size_t bLo = std::max<size_t>(2, std::min(kLo - std::min(kLo - 2, kMinHalf), size_t(double(kLo) / 1.13)));
    const size_t bHi = std::min(N / 2 - 2, std::max(kHi + kMinHalf, size_t(double(kHi) * 1.13) + 1));
    const size_t nb = bHi - bLo + 1;

    // Power per frame, pooled over 3 bins (a tone's Hann main lobe), then
    // averaged over ~kRingingSmoothS in time (evens out beating between
    // close modes and the echo pattern), in dB.
    // 4-term Blackman-Harris window (sidelobes -92 dB, main lobe +-4 bins):
    // a strong tone's own leakage stays far below the neighbourhood it is
    // compared with, so its growth can be followed to kRingingLeakDb.
    std::vector<float> win(N), re(N), im(N);
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * double(i) / double(N);
        win[i] = float(0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) - 0.01168 * std::cos(3 * x));
    }
    std::vector<float> pw(F * nb);
    for (size_t f = 0; f < F; ++f) {
        const float* x = mono.data() + segStart + f * hop;
        for (size_t i = 0; i < N; ++i) { re[i] = x[i] * win[i]; im[i] = 0.0f; }
        fft::transform(re, im, false);
        for (size_t j = 0; j < nb; ++j) {
            double p = 0.0;
            for (size_t k = bLo + j - 1; k <= bLo + j + 1; ++k) p += double(re[k]) * re[k] + double(im[k]) * im[k];
            pw[f * nb + j] = float(p);
        }
    }
    std::vector<float> lv(F * nb);
    std::vector<double> acc(nb, 0.0);
    for (size_t f = 0; f < F; ++f) {
        const size_t f0 = f > halfW ? f - halfW : 0, f1 = std::min(F - 1, f + halfW);
        for (size_t j = 0; j < nb; ++j) {
            double p = 0.0;
            for (size_t g = f0; g <= f1; ++g) p += double(pw[g * nb + j]);
            lv[f * nb + j] = float(10.0 * std::log10(std::max(p / double(f1 - f0 + 1), 1e-30)));
        }
    }
    std::vector<float> med(F), scratch;
    for (size_t f = 0; f < F; ++f) {
        scratch.assign(lv.begin() + long(f * nb + (kLo - bLo)), lv.begin() + long(f * nb + (kHi - bLo)) + 1);
        std::nth_element(scratch.begin(), scratch.begin() + long(scratch.size() / 2), scratch.end());
        med[f] = scratch[scratch.size() / 2];
    }
    // Tail start: after the loudest moment, plus one frame length and the
    // smoothing half-width, so no smoothed frame still contains the input.
    const size_t fp = size_t(std::max_element(med.begin(), med.end()) - med.begin());
    const size_t t0 = fp + 4 + halfW;
    if (t0 + 8 >= F) return r;

    // Analysis frames: up to kFrames spread over [t0, F).
    constexpr size_t kFrames = 96;
    const size_t t1 = F - 1;
    const size_t J = std::min<size_t>(kFrames, t1 - t0 + 1);
    const size_t nk = kHi - kLo + 1;
    std::vector<float> freq(nb), row(nb);
    for (size_t j = 0; j < nb; ++j) freq[j] = float(double(bLo + j) * binHz);
    std::vector<float> lev(J * nb), nbh(J * nk);
    std::vector<double> tj(J);
    for (size_t j = 0; j < J; ++j) {
        const size_t f = t0 + (J > 1 ? (j * (t1 - t0) + (J - 1) / 2) / (J - 1) : 0);
        tj[j] = double(f - t0) * hopS;
        std::copy(lv.begin() + long(f * nb), lv.begin() + long((f + 1) * nb), row.begin());
        // A bin's own level follows its peak over +-kRingingFollowBins
        // (ADR 0034 round 2): WOBBLE's pitch movement moves a mode by a bin
        // or so (8 cents at 1.3 kHz = 6 Hz = one 5.9 Hz bin), and a mode
        // drifting into a bin read as a sudden "growth" of 10-20 dB there.
        // The neighbourhood below still uses the plain bins.
        for (size_t i = 0; i < nb; ++i) {
            const size_t a = i >= kRingingFollowBins ? i - kRingingFollowBins : 0,
                         b = std::min(nb - 1, i + kRingingFollowBins);
            lev[j * nb + i] = *std::max_element(row.begin() + long(a), row.begin() + long(b) + 1);
        }
        // Neighbourhood = median of the bins within 1/3 octave (at least
        // +-kMinHalf bins), leaving out the peak's own +-kGuard bins (a tone's
        // pooled main lobe), so a strong tone never props up its own reference.
        for (size_t k = 0; k < nk; ++k) {
            const size_t c = k + kLo - bLo;
            const double fc = double(freq[c]);
            size_t lo = c, hi = c;
            while (lo > 0 && (double(freq[lo - 1]) >= fc / kThirdHalf || c - (lo - 1) <= kMinHalf)) --lo;
            while (hi + 1 < nb && (double(freq[hi + 1]) <= fc * kThirdHalf || (hi + 1) - c <= kMinHalf)) ++hi;
            scratch.clear();
            for (size_t i = lo; i <= hi; ++i)
                if (i + kGuard < c || i > c + kGuard) scratch.push_back(row[i]);
            std::nth_element(scratch.begin(), scratch.begin() + long(scratch.size() / 2), scratch.end());
            nbh[j * nk + k] = scratch[scratch.size() / 2];
        }
    }

    // Per bin: the frames where its neighbourhood is still within
    // kRingingSpanDb of where it started and well above its own floor (the
    // quietest it gets in the segment), so hum or spurs in a recording's
    // noise floor can't "grow" out of it. If the neighbourhood never drops
    // much (stationary signal, truncated IR), every frame counts. Then the
    // late half of those frames only: real tanks have an early phase where
    // the valleys between their modes drain fast (the initial broadband
    // burst), so every mode "grows" at first and then stops; a Ringing mode
    // keeps outliving its neighbours.
    // Theil-Sen slope (median of all pairwise slopes): a single bump (a
    // late echo tap, an edit at the end of an IR) can't tilt it the way it
    // tilts a least-squares line. Intercept: median of y - slope * t.
    std::vector<double> pairs;
    auto slopeOf = [&pairs](const std::vector<double>& t, const std::vector<double>& y, double& icpt) {
        pairs.clear();
        for (size_t i = 0; i < t.size(); ++i)
            for (size_t j = i + 1; j < t.size(); ++j)
                if (t[j] > t[i]) pairs.push_back((y[j] - y[i]) / (t[j] - t[i]));
        if (pairs.empty()) { icpt = y.empty() ? 0.0 : y[0]; return 0.0; }
        std::nth_element(pairs.begin(), pairs.begin() + long(pairs.size() / 2), pairs.end());
        const double sl = pairs[pairs.size() / 2];
        pairs.clear();
        for (size_t i = 0; i < t.size(); ++i) pairs.push_back(y[i] - sl * t[i]);
        std::nth_element(pairs.begin(), pairs.begin() + long(pairs.size() / 2), pairs.end());
        icpt = pairs[pairs.size() / 2];
        return sl;
    };
    double best = -1e9, bestLoud = -1e30, maxSpan = 0.0;
    std::vector<double> tt, yb, yn;
    const float audibleFloor = med[t0] - float(kRingingSteadyRangeDb);
    // No band is followed further than kRingingSpanDb below the tail's
    // typical level at its start (the median bin): a fast-dying high band
    // would otherwise be followed down to the float/dither floor, 100+ dB
    // down, where its prominence just wanders.
    const float spanFloor = med[t0] - float(kRingingSpanDb);
    for (size_t k = 0; k < nk; ++k) {
        const size_t jb = k + kLo - bLo; // index into lev rows
        auto promAt = [&](size_t j) { return double(lev[j * nb + jb]) - double(nbh[j * nk + k]); };
        float lo = 1e30f;
        for (size_t j = 0; j < J; ++j) lo = std::min(lo, nbh[j * nk + k]);
        const bool decays = nbh[k] - lo >= 25.0f;
        double score = -1e9, endP = 0.0, ratio = 1.0;

        // (1) Growth. Valid frames: the neighbourhood is still a real
        // reference, i.e. within kRingingSpanDb of where it started, well
        // above its own floor, and not yet down in this peak's own window
        // leakage (prominence < kRingingLeakDb).
        size_t last = 0;
        for (size_t j = 0; j < J; ++j) {
            const float v = nbh[j * nk + k];
            if (decays && (v < lo + 15.0f || v < nbh[k] - float(kRingingSpanDb))) break;
            if (v < spanFloor) break; // below the tail's audible range (see spanFloor)
            if (promAt(j) >= kRingingLeakDb) break;
            last = j;
        }
        const size_t first = last / 2;
        const double span = last >= first + 4 ? tj[last] - tj[first] : 0.0;
        maxSpan = std::max(maxSpan, span);
        bool peak = false;
        if (span >= kRingingMinSpanS) {
            // Narrowband peak: at least as loud as the bins around it (+-3)
            // on average over the late half.
            double own = 0.0, side = -1e30;
            for (int d = -3; d <= 3; ++d) {
                double m = 0.0;
                for (size_t j = first; j <= last; ++j) m += double(lev[j * nb + size_t(long(jb) + d)]);
                if (d == 0) own = m; else side = std::max(side, m);
            }
            peak = own >= side;
        }
        if (peak) {
            tt.clear(); yb.clear(); yn.clear();
            for (size_t j = first; j <= last; ++j) {
                tt.push_back(tj[j]);
                yb.push_back(promAt(j));
                yn.push_back(double(nbh[j * nk + k]));
            }
            double ip = 0.0, in = 0.0;
            const double sp = slopeOf(tt, yb, ip);
            const double e = ip + sp * tt.back();
            // Steady climb, not a bump: the median prominence of the late
            // half's first, middle and last thirds must rise in order (1 dB
            // slack), each step by a share of the whole rise. A Ringing mode
            // keeps pulling away; a tail that merely wobbles (two decay
            // stages handing over, beating) goes up and back down, which a
            // straight line alone could read as growth.
            auto thirdMedian = [&yb](size_t part) {
                const size_t n = yb.size(), a = part * n / 3, b = (part + 1) * n / 3;
                std::vector<double> v(yb.begin() + long(a), yb.begin() + long(std::max(b, a + 1)));
                std::nth_element(v.begin(), v.begin() + long(v.size() / 2), v.end());
                return v[v.size() / 2];
            };
            const double m1 = thirdMedian(0), m2 = thirdMedian(1), m3 = thirdMedian(2);
            // (ADR 0034 round 2) Both steps carry at least kRingingSteadyShare
            // of the rise: a single early jump (the neighbourhood draining
            // fast, then the peak decaying with it) is not a climb.
            const double rise = m3 - m1, least = kRingingSteadyShare * rise - 1.0;
            const bool climbs = rise >= 0.0 && m2 - m1 >= least && m3 - m2 >= least;
            // ...and lasts: a narrow component that dies faster than half
            // the tail's own T60 can't be heard as Ringing (it is gone while
            // the body of the tail is still sounding). This drops the fast,
            // dark top bands, where decay stages handing over read as growth.
            const double sn = slopeOf(tt, yn, in), sb = sn + sp;
            const bool lasts = std::isnan(tailT60) || tailT60 <= 0.0 || sb >= -60.0 / (kRingingMinT60Ratio * tailT60);
            if (e >= kRingingEndProminenceDb && climbs && lasts) {
                score = sp * span;
                endP = e;
                ratio = sn < -0.5 ? (sb < -1e-3 ? std::min(99.0, sn / sb) : 99.0) : 1.0;
            }
        }

        // (2) Steady tone: Ringing's end state (a self-oscillating loop, a
        // mode so slow it has left its neighbourhood behind): the peak
        // stands >= kRingingSteadyProminenceDb clear of its neighbourhood,
        // within kRingingSteadyRangeDb of the tail's start level, and itself
        // decays slower than kRingingSteadyDbPerS, over >= kRingingSteadyS.
        // Checked on the last such stretch of frames.
        size_t runEnd = J, runStart = J;
        for (size_t j = J; j-- > 0;) {
            const bool on = promAt(j) >= kRingingSteadyProminenceDb && lev[j * nb + jb] >= audibleFloor;
            if (on && runEnd == J) runEnd = j + 1;
            if (on) runStart = j;
            else if (runEnd != J) break;
        }
        if (runEnd != J && tj[runEnd - 1] - tj[runStart] >= kRingingSteadyS) {
            tt.clear(); yb.clear();
            for (size_t j = runStart; j < runEnd; ++j) { tt.push_back(tj[j]); yb.push_back(double(lev[j * nb + jb])); }
            double ib = 0.0;
            if (slopeOf(tt, yb, ib) >= -kRingingSteadyDbPerS) {
                std::vector<double> ps;
                for (size_t j = runStart; j < runEnd; ++j) ps.push_back(promAt(j));
                std::nth_element(ps.begin(), ps.begin() + long(ps.size() / 2), ps.end());
                const double steady = std::min(ps[ps.size() / 2], kRingingLeakDb); // >= 70 dB reads "a bare tone"
                if (steady > score) {
                    score = steady;
                    endP = promAt(runEnd - 1);
                    ratio = 99.0;
                }
            }
        }
        // Ties (several bins of one bare tone all at the kRingingLeakDb cap):
        // report the loudest, i.e. the tone's own bin.
        const double loud = double(lev[(J - 1) * nb + jb]);
        score = std::min(score, kRingingLeakDb); // >= 70 dB reads "a bare tone"
        if (score > -1e8 && (score > best || (score == best && loud > bestLoud))) {
            best = score;
            bestLoud = loud;
            r.db = score;
            r.endDb = endP;
            r.hz = double(kLo + k) * binHz;
            r.ratio = ratio;
        }
    }
    r.spanS = maxSpan;
    if (best < -1e8) {
        if (maxSpan >= kRingingMinSpanS) r.db = 0.0; // measurable, nothing stands out
    }
    return r;
}

// ---- M6 Howl criterion (ADR 0019) ---------------------------------------------
// Only meaningful inside the KICKED Howl zone, where it replaces the Ringing
// test. Region = [start, end) (the sustained part). See Metrics.h.
struct HowlResult {
    double floorDb = std::nan(""), movePct = std::nan(""), moveDb = std::nan("");
    bool   ok = false;
};

HowlResult howlStats(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    HowlResult h;
    size_t N = 256;
    while (double(N) * 1.5 < 0.341 * double(sr)) N <<= 1; // 16384 at 48 kHz: ~2.9 Hz bins
    if (end > mono.size() || end <= start || end - start < N + size_t(2.0 * double(sr))) return h;
    const double binHz = double(sr) / double(N);

    // Broadband floor: 1/3-octave band energies 200 Hz-5 kHz of the average
    // power spectrum; median band re the strongest band.
    std::vector<double> p(N / 2 + 1, 0.0);
    for (size_t w = start; w + N <= end; w += N / 2) {
        const auto m = fft::magnitudeSpectrum(mono.data() + w, N, N);
        for (size_t k = 0; k < p.size(); ++k) p[k] += double(m[k]) * double(m[k]);
    }
    std::vector<double> bands;
    const double half = std::pow(2.0, 1.0 / 6.0);
    for (double f = 200.0; f <= 5000.0 * 1.001; f *= std::pow(2.0, 1.0 / 3.0)) {
        double e = 0.0;
        for (size_t k = size_t(f / half / binHz); k <= size_t(f * half / binHz) && k < p.size(); ++k) e += p[k];
        bands.push_back(e);
    }
    const double strongest = *std::max_element(bands.begin(), bands.end());
    std::nth_element(bands.begin(), bands.begin() + long(bands.size() / 2), bands.end());
    if (strongest <= 0.0) return h;
    h.floorDb = 10.0 * std::log10(std::max(bands[bands.size() / 2], 1e-300) / strongest);

    // Movement: strongest peak (100 Hz-8 kHz) every 0.25 s, parabolic
    // interpolation for frequency; each 2 s window (9 snapshots) must move
    // >= 0.5 % in frequency or >= 3 dB in level while the peak is audible.
    const size_t step = size_t(0.25 * double(sr));
    std::vector<double> hz, lvl;
    for (size_t w = start; w + N <= end; w += step) {
        const auto q = fft::magnitudeSpectrum(mono.data() + w, N, N);
        size_t b = size_t(100.0 / binHz);
        for (size_t k = b; k < size_t(8000.0 / binHz) && k + 1 < q.size(); ++k)
            if (q[k] > q[b]) b = k;
        const double ym = spectral::toDb(q[b - 1]), y0 = spectral::toDb(q[b]), yp = spectral::toDb(q[b + 1]);
        const double den = ym - 2.0 * y0 + yp;
        const double off = std::fabs(den) > 1e-9 ? 0.5 * (ym - yp) / den : 0.0;
        hz.push_back((double(b) + off) * binHz);
        lvl.push_back(y0 + double(spectral::toDb(4.0f / float(N)))); // ~ tone amplitude, dBFS (Hann)
    }
    const size_t W = 9;
    if (hz.size() < W) return h;
    double worst = 1e9;
    bool moving = true;
    for (size_t i = 0; i + W <= hz.size(); ++i) {
        double fMin = 1e18, fMax = 0, lMin = 1e18, lMax = -1e18;
        for (size_t j = i; j < i + W; ++j) {
            fMin = std::min(fMin, hz[j]); fMax = std::max(fMax, hz[j]);
            lMin = std::min(lMin, lvl[j]); lMax = std::max(lMax, lvl[j]);
        }
        const double fDev = (fMax - fMin) / std::max(fMin, 1.0) * 100.0, lDev = lMax - lMin;
        if (lMax < -30.0) continue; // no audible tone in this window
        const double score = std::max(fDev / kHowlMovePct, lDev / kHowlMoveDb);
        if (score < worst) { worst = score; h.movePct = fDev; h.moveDb = lDev; }
        if (score < 1.0) moving = false;
    }
    if (worst > 1e8) { h.movePct = h.moveDb = std::nan(""); } // never audible: nothing steady
    h.ok = h.floorDb >= kHowlFloorMinDb && moving;
    return h;
}

// ---- Room-acoustics measures (reverb-testing-practices.md items 8, 11, 12) ----

// Where the first event's response starts, peaks and has died away.
//  onset:   ISO 3382-1:2009's start of an impulse response, the first sample
//           that rises to 20 dB below the segment's peak.
//  peak:    the loudest 50 ms of the segment (centred mean of five 10 ms
//           blocks). It stands in for "the input stopped": for a click it is
//           the click's response, for a burst its end, for a swell its top.
//  tailEnd: the first point after the peak where that 50 ms envelope is
//           50 dB down, or the segment's end if it never gets there.
//  quietEnd: the same at 90 dB down. The band filters stop there: what
//           follows is far below the -25 dB end of any band's fit (a band
//           would have to start 65 dB under the loudest to reach it), and
//           a 40 s render's silent end would otherwise double their FFTs.
struct TailSpan {
    size_t onset = 0, peak = 0, tailEnd = 0, quietEnd = 0;
    bool ok = false;
};

TailSpan tailSpan(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    TailSpan s;
    const size_t block = std::max<size_t>(1, size_t(0.010 * double(sr)));
    if (end > mono.size() || end <= start + 8 * block) return s;
    double maxAbs = 0.0;
    for (size_t i = start; i < end; ++i) maxAbs = std::max(maxAbs, std::fabs(double(mono[i])));
    if (maxAbs <= 0.0) return s;
    s.onset = start;
    while (s.onset < end && std::fabs(double(mono[s.onset])) < 0.1 * maxAbs) ++s.onset;

    const std::vector<double> e = blockPowers(mono, start, end, block);
    std::vector<double> env(e.size());
    for (size_t b = 0; b < e.size(); ++b) {
        const size_t lo = b >= 2 ? b - 2 : 0, hi = std::min(e.size(), b + 3);
        double sum = 0.0;
        for (size_t k = lo; k < hi; ++k) sum += e[k];
        env[b] = sum / double(hi - lo);
    }
    const size_t pk = size_t(std::max_element(env.begin(), env.end()) - env.begin());
    s.peak = std::max(s.onset, start + pk * block);
    s.tailEnd = s.quietEnd = end;
    for (size_t b = pk; b < env.size(); ++b)
        if (env[b] <= env[pk] * 1e-5) { s.tailEnd = start + b * block; break; }
    for (size_t b = pk; b < env.size(); ++b)
        if (env[b] <= env[pk] * 1e-9) { s.quietEnd = start + b * block; break; }
    s.ok = s.tailEnd > s.onset;
    return s;
}

// Double-precision radix-2 complex FFT with a shared twiddle table. The band
// filters run over a whole render (2^21 points for 40 s), where single
// precision's rounding would sit too close to the -25 dB end of the fit.
struct FftDouble {
    size_t n = 0;
    std::vector<double> twr, twi; // stage of length len: entries [len/2, len), e^{-2 pi i k / len}
    explicit FftDouble(size_t size) : n(size), twr(size), twi(size)
    {
        for (size_t half = 1; half < n; half <<= 1)
            for (size_t k = 0; k < half; ++k) {
                const double a = M_PI * double(k) / double(half);
                twr[half + k] = std::cos(a);
                twi[half + k] = -std::sin(a);
            }
    }
    void run(std::vector<double>& re, std::vector<double>& im, bool inverse) const
    {
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
        }
        const double sign = inverse ? -1.0 : 1.0;
        double* xr = re.data();
        double* xi = im.data();
        for (size_t half = 1; half < n; half <<= 1) {
            const double* wr = twr.data() + half;
            const double* wiT = twi.data() + half;
            for (size_t i = 0; i < n; i += 2 * half) {
                double* ar = xr + i; double* ai = xi + i;
                double* br = ar + half; double* bi = ai + half;
                for (size_t k = 0; k < half; ++k) {
                    const double c = wr[k], s = sign * wiT[k];
                    const double tr = br[k] * c - bi[k] * s, ti = br[k] * s + bi[k] * c;
                    br[k] = ar[k] - tr; bi[k] = ai[k] - ti;
                    ar[k] += tr;        ai[k] += ti;
                }
            }
        }
    }
};

// One band's decay, exactly as tools/ingest_reference.py's t60() measures
// the owner's recordings: backward-integrated energy (Schroeder 1965) from
// the band's loudest sample to the end of the segment; T60 = 3 x the time
// between the curve's first points below -5 dB and below -25 dB (a T20 read
// from two crossings, not a regression; NaN if it never gets to -25 dB).
// EDT is ISO 3382-1's early decay time: the least-squares line through the
// same curve from 0 to -10 dB, extrapolated to -60 dB.
void ingestDecay(const std::vector<double>& y, float sr, double& t60, double& edt)
{
    t60 = edt = std::nan("");
    size_t pk = 0;
    for (size_t i = 1; i < y.size(); ++i)
        if (std::fabs(y[i]) > std::fabs(y[pk])) pk = i;
    const size_t len = y.size() - pk;
    if (len < 8) return;
    std::vector<double> edc(len);
    double acc = 0.0;
    for (size_t i = len; i-- > 0;) { acc += y[pk + i] * y[pk + i]; edc[i] = acc; }
    if (edc[0] <= 0.0) return;
    auto d = [&](size_t i) { return 10.0 * std::log10(edc[i] / edc[0] + 1e-30); };
    // Crossings compared in power (no log per sample): d < x <=> ratio < 10^(x/10).
    const double r5 = std::pow(10.0, -0.5) - 1e-30, r10 = 0.1 - 1e-30, r25 = std::pow(10.0, -2.5) - 1e-30;
    long a = -1, b = -1, n10 = -1;
    for (size_t i = 0; i < len; ++i) {
        const double v = edc[i] / edc[0];
        if (n10 < 0 && v < r10) n10 = long(i);
        if (a < 0 && v < r5) a = long(i);
        if (v < r25) { b = long(i); break; }
    }
    if (a >= 0 && b > a) t60 = 3.0 * double(b - a) / double(sr);
    if (n10 > 1) {
        double sumT = 0, sumD = 0, sumTT = 0, sumTD = 0;
        for (long i = 0; i <= n10; ++i) {
            const double t = double(i) / double(sr), v = d(size_t(i));
            sumT += t; sumD += v; sumTT += t * t; sumTD += t * v;
        }
        const double cnt = double(n10 + 1);
        const double denom = cnt * sumTT - sumT * sumT;
        const double slope = std::fabs(denom) > 0.0 ? (cnt * sumTD - sumT * sumD) / denom : 0.0;
        if (slope < -1e-9) edt = -60.0 / slope;
    }
}

// Octave bands as tools/ingest_reference.py's octave(): a zero-phase FFT
// filter whose gain is a Gaussian in log-frequency (sigma 0.42 octave)
// around each centre, with the segment padded by a second of silence on
// each side so the filter's ringing can't wrap round the ends and floor the
// decay curve (HANDOFF "Will bite"). The FFT here is a power of two at least
// that long; the extra zeros only add padding.
// Two real bands share each inverse FFT (one in the real part, one in the
// imaginary part). Beyond 8.5 sigma (gain < 1e-15) the gain is taken as 0.
void bandDecays(const std::vector<float>& mono, size_t start, size_t end, float sr, Metrics& m)
{
    if (end > mono.size() || end <= start + size_t(0.1 * double(sr))) return;
    const size_t len = end - start, pad = size_t(std::lround(double(sr)));
    size_t n = 1;
    while (n < len + 2 * pad) n <<= 1;
    std::vector<double> re(n, 0.0), im(n, 0.0);
    for (size_t i = 0; i < len; ++i) re[pad + i] = double(mono[start + i]);
    const FftDouble fftN(n);
    fftN.run(re, im, false);

    constexpr double kSigmaOct = 0.42, kCutSigma = 8.5;
    std::vector<double> log2f(n / 2 + 1); // log2 of each bin's frequency
    for (size_t k = 0; k <= n / 2; ++k) log2f[k] = std::log2(std::max(double(k) * double(sr) / double(n), 1e-3));
    auto weight = [&](size_t bin, int band) {
        const double o = (log2f[bin] - std::log2(Metrics::bandHz[band])) / kSigmaOct;
        return std::fabs(o) > kCutSigma ? 0.0 : std::exp(-0.5 * o * o);
    };
    std::vector<double> yr(n), yi(n), band(len);
    for (int b = 0; b < Metrics::kNumBands; b += 2) {
        const bool two = b + 1 < Metrics::kNumBands;
        for (size_t k = 0; k < n; ++k) {
            const size_t bin = k <= n / 2 ? k : n - k; // negative frequencies mirror
            const double w1 = weight(bin, b), w2 = two ? weight(bin, b + 1) : 0.0;
            yr[k] = re[k] * w1 - im[k] * w2;
            yi[k] = im[k] * w1 + re[k] * w2;
        }
        fftN.run(yr, yi, true);
        for (size_t i = 0; i < len; ++i) band[i] = yr[pad + i] / double(n);
        ingestDecay(band, sr, m.bandT60S[b], m.bandEdtS[b]);
        if (two) {
            for (size_t i = 0; i < len; ++i) band[i] = yi[pad + i] / double(n);
            ingestDecay(band, sr, m.bandT60S[b + 1], m.bandEdtS[b + 1]);
        }
    }
}

// Interaural cross-correlation coefficient, ISO 3382-1:2009 (binaural
// measures): IACF(tau) = sum L(t) R(t + tau) / sqrt(sum L^2 * sum R^2) over
// [a, b), IACC = max |IACF| for |tau| <= 1 ms. ISO means a dummy head's two
// ears; on our L/R outputs it reads as "how alike are the two sides".
double iacc(const std::vector<float>& L, const std::vector<float>& R, size_t a, size_t b, float sr)
{
    const size_t frames = std::min(L.size(), R.size());
    b = std::min(b, frames);
    if (b <= a + 8) return std::nan("");
    double eL = 0.0, eR = 0.0;
    for (size_t t = a; t < b; ++t) { eL += double(L[t]) * L[t]; eR += double(R[t]) * R[t]; }
    if (eL <= 0.0 || eR <= 0.0) return std::nan("");
    // Long spans (a late part of many seconds) are sampled every `stride`
    // samples, at least 240 000 products per lag: the coefficient's estimate
    // is then within ~0.002 of the full sum, at a fraction of 97 full passes.
    const long stride = std::max<long>(1, long(b - a) / 240000);
    if (stride > 1) {
        eL = eR = 0.0;
        for (size_t t = a; t < b; t += size_t(stride)) { eL += double(L[t]) * L[t]; eR += double(R[t]) * R[t]; }
        if (eL <= 0.0 || eR <= 0.0) return std::nan("");
    }
    const long maxLag = std::lround(0.001 * double(sr));
    double best = 0.0;
    for (long tau = -maxLag; tau <= maxLag; ++tau) {
        long lo = std::max(long(a), -tau);
        lo += (stride - (lo - long(a)) % stride) % stride; // stay on the energy sums' grid
        const long hi = std::min(long(b), long(frames) - tau);
        double s = 0.0;
        for (long t = lo; t < hi; t += stride) s += double(L[size_t(t)]) * double(R[size_t(t + tau)]);
        best = std::max(best, std::fabs(s));
    }
    return best / std::sqrt(eL * eR);
}

// Late-tail flatness: the broadband Schroeder curve from the loudest sample
// to the segment's end, its least-squares line between -5 and -35 dB (the
// T30 range of ISO 3382-1), and the RMS of the curve's deviation from that
// line, in dB. A clean exponential tail is a straight line (~0 dB); a tail
// that pumps, beats or has a double slope bends away from it. The decay
// curve itself is what ISO's "curvature" (T30/T20) also looks at.
double tailFlatnessDb(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    if (end > mono.size() || end <= start + 8) return std::nan("");
    size_t pk = start;
    for (size_t i = start; i < end; ++i)
        if (std::fabs(mono[i]) > std::fabs(mono[pk])) pk = i;
    const size_t len = end - pk;
    std::vector<double> edc(len);
    double acc = 0.0;
    for (size_t i = len; i-- > 0;) { acc += double(mono[pk + i]) * double(mono[pk + i]); edc[i] = acc; }
    if (len < 8 || edc[0] <= 0.0) return std::nan("");
    auto d = [&](size_t i) { return 10.0 * std::log10(edc[i] / edc[0] + 1e-300); };
    long n5 = -1, n35 = -1;
    const double r5 = std::pow(10.0, -0.5) * edc[0], r35 = std::pow(10.0, -3.5) * edc[0];
    for (size_t i = 0; i < len; ++i) {
        if (n5 < 0 && edc[i] <= r5) n5 = long(i);
        if (edc[i] <= r35) { n35 = long(i); break; }
    }
    if (n5 < 0 || n35 <= n5 + 8) return std::nan("");
    double sumT = 0, sumD = 0, sumTT = 0, sumTD = 0;
    for (long i = n5; i <= n35; ++i) {
        const double t = double(i) / double(sr), v = d(size_t(i));
        sumT += t; sumD += v; sumTT += t * t; sumTD += t * v;
    }
    const double cnt = double(n35 - n5 + 1);
    const double denom = cnt * sumTT - sumT * sumT;
    if (std::fabs(denom) <= 0.0) return std::nan("");
    const double slope = (cnt * sumTD - sumT * sumD) / denom, icept = (sumD - slope * sumT) / cnt;
    double sq = 0.0;
    for (long i = n5; i <= n35; ++i) {
        const double r = d(size_t(i)) - (icept + slope * double(i) / double(sr));
        sq += r * r;
    }
    return std::sqrt(sq / cnt);
}

// echoDensityGrainS, echoDensityTimeS and echoDensityLate for one channel
// (see roomMeasures).
void echoDensityOf(const std::vector<float>& x, const TailSpan& span, size_t grainOnset, float sr, double& grainS,
                   double& timeS, double& late)
{
    grainS = timeS = late = std::nan("");
    const size_t hop = std::max<size_t>(1, size_t(std::lround(0.001 * double(sr))));
    // The profile every 1 ms over the first kEchoGrainSpanS after the onset
    // (the start of the tail; every SPACE's profile has settled by then).
    const size_t grainEnd = std::min(span.tailEnd, grainOnset + size_t(kEchoGrainSpanS * double(sr)));
    const size_t firstEnd = std::min(span.tailEnd, span.onset + size_t(4.0 * double(sr)));
    // The raw profile of Gaussian noise itself dips below 0.9 now and then
    // (3-10 % of the time with a 20 ms window, more the darker the noise:
    // one-pole low-passed noise at ~150 Hz took 0.77 s to "hold" 100 ms
    // >= 0.9), so both figures read the profile's centred 100 ms mean,
    // which Gaussian noise keeps >= 0.9 (tested in test_density).
    const size_t hold = size_t(std::lround(kEchoDensityHoldS * double(sr) / double(hop)));
    auto smoothed = [&](const std::vector<double>& p) {
        const size_t h = hold / 2;
        std::vector<double> prefix(p.size() + 1, 0.0), s(p.size());
        for (size_t k = 0; k < p.size(); ++k) prefix[k + 1] = prefix[k] + p[k];
        for (size_t k = 0; k < p.size(); ++k) {
            const size_t lo = k >= h ? k - h : 0, hi = std::min(p.size(), k + h + 1);
            s[k] = (prefix[hi] - prefix[lo]) / double(hi - lo);
        }
        return s;
    };
    // Grain: the density missing below the bar, integrated over the span and
    // scaled so that NED 0 counts one second per second,
    //   grain = integral of max(0, 0.9 - mean100(t)) / 0.9 dt.
    // Abel and Huang (2006, sec. 4.1) read the start of the late field where
    // the profile first reaches 1, or comes within its late-field standard
    // deviation of 1 to allow for the profile's own fluctuation; Lindau,
    // Kosanke and Weinzierl (JAES 60(11), 2012) found that first-reaches-1
    // time (their criterion I, mean of both ears) the best signal-based
    // predictor of the perceptual mixing time. Both are first crossings,
    // which jump whenever a profile hovers near the bar (MASSIVE's sits at
    // 0.85-0.95 for seconds; its crossing read 0.06 s to 1.67 s, and
    // criterion I 0.14 s to 2.57 s, across one-sample changes of the input
    // click). The area keeps a bar and our smoothing but sums how far below
    // it the profile sits and for how long, so it moves smoothly with the
    // signal: across single-sample clicks of other levels, polarity, time
    // and block position it moved by <= 0.011 s in all 18 grainy-gate
    // settings (the crossing by up to 0.68 s). A profile that steps from 0
    // to 1 at T reads T, as the crossing would. The bar is 0.9 rather than 1
    // so the late field's own fluctuation adds nothing (Abel and Huang's
    // reason for criterion II); Gaussian and low-passed noise read 0.
    // Known dependence: echo density counts samples, so a duller excitation
    // reads smoother (a two-sample click: up to 0.045 s less grain).
    if (grainEnd > grainOnset) {
        const std::vector<double> g = smoothed(echoDensityProfile(x, grainOnset, grainEnd, sr, hop));
        double missing = 0.0;
        for (double v : g) missing += std::max(0.0, kEchoDensityTarget - v) / kEchoDensityTarget;
        grainS = missing * double(hop) / double(sr);
    }
    // The deprecated crossing (Metrics.h), unchanged: from span.onset, over
    // its first 4 s, then the whole tail if it isn't reached by then.
    std::vector<double> s = smoothed(echoDensityProfile(x, span.onset, firstEnd, sr, hop));
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            if (!std::isnan(timeS) || firstEnd >= span.tailEnd) break;
            s = smoothed(echoDensityProfile(x, span.onset, span.tailEnd, sr, hop));
        }
        size_t run = 0;
        for (size_t k = 0; k < s.size(); ++k) {
            run = s[k] >= kEchoDensityTarget ? run + 1 : 0;
            if (run > hold) { timeS = double((k - hold) * hop) / double(sr); break; }
        }
    }
    // The late mean only needs a fair sample of the profile: every 5 ms.
    const size_t lateFrom = std::max(span.onset, span.peak + size_t(0.3 * double(sr)));
    const std::vector<double> lateNed = echoDensityProfile(x, lateFrom, span.tailEnd, sr, 5 * hop);
    double sum = 0.0;
    for (double v : lateNed) sum += v;
    if (!lateNed.empty()) late = sum / double(lateNed.size());
}

// All of the room-acoustics measures on the first event's segment.
//  - Echo density (item 8): on each of the first two analysis channels
//    (the mono downmix of decorrelated sides would read denser than either
//    ear hears). echoDensityGrainS is the mean of the two sides, as Lindau
//    et al. 2012 average both ears' mixing times (neither ear predicted
//    better than the other); echoDensityLate keeps the lower side, the
//    deprecated echoDensityTimeS the later. The 0.9 bar on the profile's
//    100 ms mean is ours (Abel and Huang read the mixing time where the
//    profile reaches ~1; see echoDensityOf). The late span starts 0.3 s
//    after the input stopped (TailSpan.peak).
//  - Band T60 / EDT (item 11, 12): bandDecays, from the first event to
//    TailSpan.quietEnd.
//  - C80 (ISO 3382-1): 10 log10(energy onset..+80 ms / energy +80 ms..end),
//    on the mono downmix.
//  - IACC early (onset..+80 ms) and late (+80 ms..tail end), stereo only.
//  - Tail flatness: tailFlatnessDb.
void roomMeasures(const std::vector<std::vector<float>>& channels, const std::vector<float>& mono,
                  const std::vector<std::vector<float>>& stereoCh, size_t segStart, size_t segEnd,
                  float sr, Metrics& m)
{
    m.tailFlatnessDb = tailFlatnessDb(mono, segStart, segEnd, sr);
    const TailSpan span = tailSpan(mono, segStart, segEnd, sr);
    bandDecays(mono, segStart, span.ok ? span.quietEnd : segEnd, sr, m);
    if (!span.ok) return;

    // The grain starts at the same ISO onset rule (20 dB below the segment's
    // peak), searched from the start of the file instead of from the first
    // detected event: findEvents can place a reverb's first event late, on
    // the swell rather than the quiet taps before it, and where it lands
    // moves with trivial input changes (MASSIVE SIZE 0 / DECAY 0.7: 0.34 s
    // for a click at 0.5, 0.78 s at 0.9), cutting off part of the grainy
    // start. Searched from 0, the onset held still for every click variant.
    // (The other room measures, and the deprecated crossing, keep span.onset.)
    size_t grainOnset = span.onset;
    {
        double maxAbs = 0.0;
        for (size_t i = segStart; i < segEnd; ++i) maxAbs = std::max(maxAbs, std::fabs(double(mono[i])));
        for (size_t i = 0; i < span.onset; ++i)
            if (std::fabs(double(mono[i])) >= 0.1 * maxAbs) { grainOnset = i; break; }
    }

    const size_t numCh = std::min<size_t>(2, channels.size());
    for (size_t c = 0; c < std::max<size_t>(1, numCh); ++c) {
        double g = 0.0, t = 0.0, l = 0.0;
        echoDensityOf(numCh >= 2 ? channels[c] : mono, span, grainOnset, sr, g, t, l);
        if (c == 0) { m.echoDensityGrainS = g; m.echoDensityTimeS = t; m.echoDensityLate = l; continue; }
        m.echoDensityGrainS = 0.5 * (m.echoDensityGrainS + g); // NaN stays NaN
        m.echoDensityTimeS = (std::isnan(t) || std::isnan(m.echoDensityTimeS)) ? std::nan("")
                                                                              : std::max(t, m.echoDensityTimeS);
        m.echoDensityLate = (std::isnan(l) || std::isnan(m.echoDensityLate)) ? std::nan("")
                                                                            : std::min(l, m.echoDensityLate);
    }

    const size_t split = span.onset + size_t(std::lround(0.080 * double(sr)));
    if (split < segEnd) {
        double early = 0.0, after = 0.0;
        for (size_t i = span.onset; i < split; ++i) early += double(mono[i]) * double(mono[i]);
        for (size_t i = split; i < segEnd; ++i) after += double(mono[i]) * double(mono[i]);
        if (early > 0.0 && after > 0.0) m.c80Db = 10.0 * std::log10(early / after);
    }
    if (stereoCh.size() >= 2 && split < span.tailEnd) {
        m.iaccEarly = iacc(stereoCh[0], stereoCh[1], span.onset, split, sr);
        m.iaccLate = iacc(stereoCh[0], stereoCh[1], split, span.tailEnd, sr);
    }
}

} // namespace

// Normalized echo density profile (Abel and Huang 2006, "A simple, robust
// measure of reverberation echo density", AES 121st Convention):
//   eta(t) = 1/erfc(1/sqrt 2) * sum_tau w(tau) 1{|h(t + tau)| > sigma(t)},
//   sigma(t) = sqrt(sum_tau w(tau) h^2(t + tau)),
// with w a Hann window (20 ms here, as in the paper) normalised to sum 1.
// erfc(1/sqrt 2) = 0.3173 is the share of Gaussian samples outside one
// standard deviation, so Gaussian noise reads 1 and sparse echoes read near
// 0. (github.com/microsoft/echodensity, the repo our research notes point
// at, implements a later measure of the same thing, Tukuljac et al.'s
// Sorted Center Time, ICASSP 2019; it is cited for its onset handling and
// noise normalisation, not copied.) Near the ends of `x` the window is cut
// and renormalised.
std::vector<double> echoDensityProfile(const std::vector<float>& x, size_t start, size_t end, float sampleRate,
                                       size_t hop, double windowS)
{
    std::vector<double> out;
    end = std::min(end, x.size());
    if (hop == 0 || end <= start) return out;
    size_t L = size_t(std::lround(windowS * double(sampleRate)));
    if (L % 2 == 0) ++L;
    const size_t half = L / 2;
    std::vector<double> w(L);
    for (size_t k = 0; k < L; ++k) w[k] = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(k + 1) / double(L + 1));
    const double norm = 1.0 / std::erfc(1.0 / std::sqrt(2.0));
    out.reserve((end - start) / hop + 1);
    for (size_t p = start; p < end; p += hop) {
        const size_t lo = p >= half ? p - half : 0, hi = std::min(x.size(), p + half + 1);
        const size_t w0 = lo + half - p; // window index of sample lo
        double wsum = 0.0, s2 = 0.0;
        for (size_t i = lo; i < hi; ++i) {
            const double wi = w[w0 + i - lo];
            wsum += wi;
            s2 += wi * double(x[i]) * double(x[i]);
        }
        if (s2 <= 0.0 || wsum <= 0.0) { out.push_back(0.0); continue; }
        const double sigma = std::sqrt(s2 / wsum);
        double outside = 0.0;
        for (size_t i = lo; i < hi; ++i)
            if (std::fabs(double(x[i])) > sigma) outside += w[w0 + i - lo];
        out.push_back(norm * outside / wsum);
    }
    return out;
}

Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate,
                 const std::vector<std::vector<float>>* stereoSource)
{
    Metrics m;
    const size_t numCh = channels.size();
    const size_t frames = numCh ? channels[0].size() : 0;

    for (const auto& ch : channels) {
        for (float x : ch) {
            if (!std::isfinite(x)) ++m.nanInfCount;
            else if (std::fabs(x) >= 0.999f) ++m.clipCount;
        }
    }

    std::vector<float> mono(frames, 0.0f);
    for (size_t i = 0; i < frames; ++i) {
        double sum = 0.0;
        for (const auto& ch : channels) {
            const float v = ch[i];
            if (std::isfinite(v)) sum += double(v);
        }
        mono[i] = numCh ? float(sum / double(numCh)) : 0.0f;
    }

    double peak = 0.0, sumSq = 0.0;
    for (float x : mono) {
        const double a = std::fabs(double(x));
        if (a > peak) peak = a;
        sumSq += double(x) * double(x);
    }
    m.peakDbfs = peak > 0.0 ? double(spectral::toDb(float(peak))) : -200.0;
    const double rms = frames ? std::sqrt(sumSq / double(frames)) : 0.0;
    m.rmsDbfs = rms > 0.0 ? double(spectral::toDb(float(rms))) : -200.0;

    const auto events = findEvents(mono, sampleRate);
    size_t segStart = 0, segEnd = frames;
    if (!events.empty()) {
        segStart = events[0];
        segEnd = events.size() > 1 ? events[1] : frames;
    }
    // T60 stops before the next event's build-up (decayEnd above); the
    // other segment metrics keep the full [first event, second event). A
    // recording's noise floor, estimated over the whole segment, bounds the
    // fit too (schroederT60 above).
    const size_t t60End = events.size() > 1 ? decayEnd(mono, segStart, segEnd, sampleRate) : segEnd;
    const double floorPower = noiseFloorPower(mono, segStart, segEnd, sampleRate);
    m.t60S = schroederT60(mono, segStart, t60End, sampleRate, floorPower);

    // Resonance segment starts 1 s after the first event (or 1 s into the
    // file when no event is found, e.g. analyzing a sustained reference
    // recording with --analyze).
    size_t resStart = events.empty() ? std::min(frames, size_t(1.0 * double(sampleRate)))
                                      : std::min(segEnd, events[0] + size_t(1.0 * double(sampleRate)));
    resStart = std::min(resStart, segEnd);
    m.resonancePeakDb = resonancePeakDb(mono, resStart, segEnd, sampleRate);

    m.steadyTone = steadyTone(mono, sampleRate);

    const RingingResult ring = ringingGrowth(mono, segStart, segEnd, sampleRate, m.t60S);
    m.ringingDb    = ring.db;
    m.ringingHz    = ring.hz;
    m.ringingRatio = ring.ratio;
    m.ringingEndDb = ring.endDb;
    m.ringingSpanS = ring.spanS;
    m.ringing      = !std::isnan(ring.db) && ring.db >= kRingingGrowthDb;

    // The Howl never really stops, so its region runs to the end of the file
    // (a dip under -40 dBFS mid-Howl would otherwise end the segment).
    const HowlResult howl = howlStats(mono, resStart, frames, sampleRate);
    m.howlFloorDb = howl.floorDb;
    m.howlMovePct = howl.movePct;
    m.howlMoveDb  = howl.moveDb;
    m.howlOk      = howl.ok;
    m.clickCount = clickCount(mono, sampleRate);
    m.maxStepDb100ms = maxStepDb100ms(mono, sampleRate, events);

    const auto& stereoCh = stereoSource ? *stereoSource : channels;
    // Echo density, band decay, C80, IACC, tail flatness: over the same
    // span as T60 (first event to before the next one's build-up).
    {
        static const std::vector<std::vector<float>> kNoStereo;
        const bool stereoOk = stereoCh.size() >= 2 && stereoCh[0].size() >= frames && stereoCh[1].size() >= frames;
        roomMeasures(channels, mono, stereoOk ? stereoCh : kNoStereo, segStart, t60End, sampleRate, m);
    }
    if (stereoCh.size() >= 2 && !stereoCh[0].empty() && !stereoCh[1].empty()) {
        const auto& L = stereoCh[0];
        const auto& R = stereoCh[1];
        const size_t sFrames = std::min(L.size(), R.size());
        const size_t corrStart = std::min(segStart, sFrames), corrEnd = std::min(segEnd, sFrames);
        m.stereoCorrelation = pearsonCorrelation(L, R, corrStart, corrEnd);
        m.monoLossDb = monoLossDb(L, R);
        m.monoNotchDb = monoNotchDb(L, R, corrStart, corrEnd, sampleRate);
    }
    return m;
}

std::string summaryLine(const Metrics& m)
{
    char t60Buf[32], resBuf[32], corrBuf[32], lossBuf[32], notchBuf[32], stepBuf[32], ringBuf[64], howlBuf[64], buf[1024];
    if (std::isnan(m.ringingDb)) std::snprintf(ringBuf, sizeof ringBuf, "null");
    else std::snprintf(ringBuf, sizeof ringBuf, "%.1fdB@%.0fHz,x%.2f", m.ringingDb, m.ringingHz, m.ringingRatio);
    if (std::isnan(m.howlFloorDb)) std::snprintf(howlBuf, sizeof howlBuf, "null");
    else std::snprintf(howlBuf, sizeof howlBuf, "floor%.0fdB/move%.2f%%,%.1fdB", m.howlFloorDb, m.howlMovePct, m.howlMoveDb);
    if (std::isnan(m.t60S)) std::snprintf(t60Buf, sizeof t60Buf, "null");
    else std::snprintf(t60Buf, sizeof t60Buf, "%.2fs", m.t60S);
    if (std::isnan(m.resonancePeakDb)) std::snprintf(resBuf, sizeof resBuf, "null");
    else std::snprintf(resBuf, sizeof resBuf, "%.1fdB", m.resonancePeakDb);
    if (std::isnan(m.stereoCorrelation)) std::snprintf(corrBuf, sizeof corrBuf, "mono");
    else std::snprintf(corrBuf, sizeof corrBuf, "%.2f", m.stereoCorrelation);
    if (std::isnan(m.monoLossDb)) std::snprintf(lossBuf, sizeof lossBuf, "mono");
    else std::snprintf(lossBuf, sizeof lossBuf, "%.1fdB", m.monoLossDb);
    if (std::isnan(m.monoNotchDb)) std::snprintf(notchBuf, sizeof notchBuf, "mono");
    else std::snprintf(notchBuf, sizeof notchBuf, "%.1fdB", m.monoNotchDb);
    if (std::isnan(m.maxStepDb100ms)) std::snprintf(stepBuf, sizeof stepBuf, "null");
    else std::snprintf(stepBuf, sizeof stepBuf, "%.1fdB", m.maxStepDb100ms);
    std::snprintf(buf, sizeof buf,
        "peak=%.1fdBFS rms=%.1fdBFS t60=%s res=%s steady=%s nan/inf=%ld clip=%ld click=%ld "
        "corr=%s monoloss=%s notch=%s maxstep=%s ringing=%s%s howl=%s",
        m.peakDbfs, m.rmsDbfs, t60Buf, resBuf,
        m.steadyTone ? "true" : "false", m.nanInfCount, m.clipCount, m.clickCount,
        corrBuf, lossBuf, notchBuf, stepBuf, ringBuf, m.ringing ? "(RINGING)" : "", howlBuf);
    // Room-acoustics measures, appended (the band arrays live in the sidecar).
    auto fmt = [](double x, const char* f, const char* nullText) {
        char b[32];
        if (std::isnan(x)) std::snprintf(b, sizeof b, "%s", nullText);
        else std::snprintf(b, sizeof b, f, x);
        return std::string(b);
    };
    std::string line(buf);
    line += " grain=" + fmt(m.echoDensityGrainS, "%.3fs", "null") + " late_density=" + fmt(m.echoDensityLate, "%.2f", "null");
    line += " c80=" + fmt(m.c80Db, "%.1fdB", "null");
    line += " iacc=" + fmt(m.iaccEarly, "%.2f", "mono") + "/" + fmt(m.iaccLate, "%.2f", "mono");
    line += " flat=" + fmt(m.tailFlatnessDb, "%.2fdB", "null");
    return line;
}

} // namespace iv::metrics
