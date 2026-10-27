#pragma once
// The listening pages' pitch measure in C++ (tools/pages/drift_rethink.py cents_tracks / pitch_stats): a tone's
// pitch, channel by channel, as the phase slope of the band ±150 Hz around it (an analytic signal), in 1 ms frames,
// smoothed to 40 Hz, 100 ms trimmed each end; p95Cents is the page's "p95": the channels' mean, |cents| about its
// median, 95th percentile. Used by test_motion (MOTION's depth) and test_room_motion (the small room's).

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace iv::test {

inline void fft(std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * M_PI / double(len) * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse)
        for (auto& x : a) x /= double(n);
}

// One channel's pitch, cents from its median, 1 ms frames over t0..t1 seconds.
inline std::vector<double> centsTrack(const std::vector<float>& x, double f0, double sr, double t0 = 3.0, double t1 = 9.0)
{
    using cplx = std::complex<double>;
    const size_t n0 = size_t(t0 * sr), n1 = std::min(x.size(), size_t(t1 * sr)), len = n1 - n0;
    size_t nf = 1;
    while (nf < len) nf <<= 1;
    std::vector<cplx> X(nf);
    for (size_t i = 0; i < len; ++i) X[i] = x[n0 + i];
    fft(X, false);
    for (size_t k = 0; k < nf; ++k) {
        const double f = double(k) * sr / double(nf);
        X[k] = (k > nf / 2 || std::fabs(f - f0) > 150.0) ? cplx(0.0) : 2.0 * X[k];
    }
    fft(X, true);
    const int frame = int(sr / 1000.0);
    std::vector<double> inst;
    double prev = std::arg(X[0]), acc = 0.0;
    int cnt = 0;
    for (size_t i = 1; i < len; ++i) {
        const double ph = std::arg(X[i]);
        double d = ph - prev;
        prev = ph;
        while (d > M_PI) d -= 2.0 * M_PI;
        while (d < -M_PI) d += 2.0 * M_PI;
        acc += d * sr / (2.0 * M_PI);
        if (++cnt == frame) {
            inst.push_back(acc / double(frame));
            acc = 0.0;
            cnt = 0;
        }
    }
    size_t m = 1;
    while (m < inst.size()) m <<= 1;
    std::vector<cplx> Y(m, cplx(0.0));
    double mean = 0.0;
    for (double v : inst) mean += v;
    mean /= double(inst.size());
    for (size_t i = 0; i < inst.size(); ++i) Y[i] = inst[i] - mean;
    fft(Y, false);
    for (size_t k = 0; k < m; ++k) {
        const size_t kk = k <= m / 2 ? k : m - k;
        if (double(kk) * 1000.0 / double(m) > 40.0) Y[k] = 0.0;
    }
    fft(Y, true);
    std::vector<double> c;
    for (size_t i = 100; i + 100 < inst.size(); ++i) c.push_back(Y[i].real() + mean);
    std::vector<double> s(c);
    std::nth_element(s.begin(), s.begin() + long(s.size() / 2), s.end());
    const double med = s[s.size() / 2];
    for (auto& v : c) v = 1200.0 * std::log2(v / med);
    return c;
}

// The page's p95: the two channels' mean, |cents| about its median, 95th percentile.
inline double p95Cents(const std::vector<float>& l, const std::vector<float>& r, double f0, double sr, double t0 = 3.0,
                       double t1 = 9.0)
{
    const auto a = centsTrack(l, f0, sr, t0, t1), b = centsTrack(r, f0, sr, t0, t1);
    std::vector<double> m(a.size());
    for (size_t i = 0; i < a.size(); ++i) m[i] = 0.5 * (a[i] + b[i]);
    std::vector<double> sorted(m);
    std::nth_element(sorted.begin(), sorted.begin() + long(sorted.size() / 2), sorted.end());
    const double med = sorted[sorted.size() / 2];
    for (auto& v : m) v = std::fabs(v - med);
    std::sort(m.begin(), m.end());
    return m[size_t(0.95 * double(m.size() - 1))];
}

} // namespace iv::test
