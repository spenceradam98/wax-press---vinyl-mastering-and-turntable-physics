// Small DSP building blocks. Plain C++17, scalar code only (no SIMD intrinsics, no -march flags),
// so it runs on any x86-64 (macOS 11 Intel) or arm64 machine.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace wp
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g, double floorDb = -120.0) { return g > 0 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb; }
inline double clampd (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Biquad, transposed direct form II. RBJ cookbook designs (bilinear with pre-warping).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void norm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static double w0 (double fs, double fc) { return kTwoPi * clampd (fc, 2.0, fs * 0.47) / fs; }

    void lowpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void allpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double fc, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    // Complex response at frequency f (for tests and the UI).
    std::complex<double> response (double fs, double f) const
    {
        const auto z1c = std::polar (1.0, -kTwoPi * f / fs), z2c = z1c * z1c;
        return (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
    }
};

struct OnePoleLP
{
    double a = 0, z = 0;
    void set (double fs, double fc) { a = std::exp (-kTwoPi * clampd (fc, 0.01, fs * 0.49) / fs); }
    void setTime (double fs, double seconds) { a = seconds <= 0 ? 0 : std::exp (-1.0 / (seconds * fs)); }
    inline double process (double x) { z = x + a * (z - x); return z; }
    void reset (double v = 0) { z = v; }
};

struct DcBlocker
{
    double x1 = 0, y1 = 0, r = 0.9995;
    void set (double fs, double fc = 5.0) { r = std::exp (-kTwoPi * fc / fs); }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Linear-in-time parameter smoother (one-pole towards the target).
struct Smoothed
{
    double cur = 0, target = 0, a = 0;
    void set (double fs, double seconds) { a = std::exp (-1.0 / (seconds * fs)); }
    void reset (double v) { cur = target = v; }
    inline double next() { cur = target + a * (cur - target); return cur; }
};

// Deterministic xorshift RNG (no allocation, real-time safe). Same generator as platterbox.
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline double uni() { return (next() >> 8) * (1.0 / 16777216.0); }   // [0,1)
    inline double bi() { return uni() * 2.0 - 1.0; }                      // [-1,1)
};

// Smoothly wandering random value, roughly unit RMS (two cascaded one-poles on white noise,
// calibrated at prepare time). From platterbox.
struct Drift
{
    OnePoleLP a, b;
    Rng rng;
    double scale = 1.0;
    void set (double fs, double fc, uint32_t seed)
    {
        a.set (fs, fc); b.set (fs, fc); rng = Rng (seed);
        a.reset(); b.reset();
        scale = 1.0;
        double acc = 0; const int n = (int) (fs * 3.0);
        for (int i = 0; i < n; ++i) { const double v = process(); if (i > n / 3) acc += v * v; }
        const double rms = std::sqrt (acc / (double) (n - n / 3 - 1));
        scale = rms > 1e-12 ? 1.0 / rms : 1.0;
    }
    inline double process() { return b.process (a.process (rng.bi())) * scale; }
};

// Topology-preserving state-variable filter (Zavalishin), used as the crackle resonators.
struct Svf
{
    double g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (double fs, double fc, double q)
    {
        g = std::tan (kPi * clampd (fc, 10.0, fs * 0.45) / fs);
        k = 1.0 / std::max (0.3, q);
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline double bandpass (double x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v1;
    }
};

// Linkwitz-Riley 4th-order crossover. lp + hp equals the 2nd-order Butterworth-Q allpass that
// allpass() returns, so a path that is not split can be phase matched with that allpass.
struct LR4
{
    Biquad lp1, lp2, hp1, hp2, ap;
    void set (double fs, double fc)
    {
        const double q = std::sqrt (0.5);
        lp1.lowpass (fs, fc, q); lp2 = lp1;
        hp1.highpass (fs, fc, q); hp2 = hp1;
        ap.allpass (fs, fc, q);
        // keep the states: coefficient changes happen while running
    }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); ap.reset(); }
    inline double low (double x) { return lp2.process (lp1.process (x)); }
    inline double high (double x) { return hp2.process (hp1.process (x)); }
    inline double allpass (double x) { return ap.process (x); }
};

// 4-point, 3rd-order Hermite interpolation.
inline double hermite (double xm1, double x0, double x1, double x2, double t)
{
    const double c = (x1 - xm1) * 0.5;
    const double v = x0 - x1;
    const double w = c + v;
    const double a = w + v + (x2 - x0) * 0.5;
    const double bNeg = w + a;
    return (((a * t) - bNeg) * t + c) * t + x0;
}

// Power-of-two circular delay line with fractional (Hermite) reads.
struct DelayLine
{
    std::vector<double> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 4; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0); mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); w = 0; }
    inline void push (double x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay in samples (0 = the sample pushed last); fractional reads need d >= 1
    inline double read (double d) const
    {
        const double pos = (double) (w - 1) - d;
        const double fl = std::floor (pos);
        const int i = (int) fl;
        const double t = pos - fl;
        auto at = [this] (int k) { return buf[(size_t) (k & mask)]; };
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
    }
    inline double readInt (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
};
} // namespace wp
