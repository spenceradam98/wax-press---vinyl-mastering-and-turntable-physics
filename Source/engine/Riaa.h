// RIAA recording (pre-emphasis) and playback (de-emphasis) filters.
//
// The playback curve is the standard three time constants: 3180 us, 318 us, 75 us,
//     H(s) = (1 + s*t2) / ((1 + s*t1) (1 + s*t3)),   normalised to 0 dB at 1 kHz.
//
// A bilinear design puts a zero at Nyquist, and the recording filter (its inverse) would then
// have a pole on the unit circle. So the digital playback filter is built from matched poles
// and zero (z = exp(-T/t)) plus one extra real zero z0 that is fitted at prepare time to the
// analog curve (a 1-D search, minimax dB error from 20 Hz to 20 kHz or 0.45 fs). Every
// pole and zero is then strictly inside the unit circle, so the recording filter is exactly
// the inverse biquad and is stable: at perfect accuracy cut -> playback is flat to rounding.
//
// "Accuracy" detunes the playback time constants the way a cheap phono preamp does
// (component tolerance), keeping the same z0, and adds a steeper subsonic high-pass.
#pragma once

#include "Dsp.h"

namespace wp
{
struct RiaaDesign
{
    static constexpr double t1 = 3180e-6, t2 = 318e-6, t3 = 75e-6;

    // analog playback magnitude, linear, normalised to 1 kHz
    static double analogPlayback (double f, double T1 = t1, double T2 = t2, double T3 = t3)
    {
        auto mag = [&] (double fr)
        {
            const std::complex<double> s (0.0, kTwoPi * fr);
            return std::abs ((1.0 + s * T2) / ((1.0 + s * T1) * (1.0 + s * T3)));
        };
        return mag (f) / mag (1000.0);
    }

    // Digital playback biquad for time constants T1..T3 with extra zero z0, normalised at 1 kHz.
    static Biquad playback (double fs, double z0, double T1 = t1, double T2 = t2, double T3 = t3)
    {
        const double p1 = std::exp (-1.0 / (T1 * fs)), p3 = std::exp (-1.0 / (T3 * fs));
        const double zz = std::exp (-1.0 / (T2 * fs));
        Biquad b;
        b.b0 = 1.0; b.b1 = -(zz + z0); b.b2 = zz * z0;
        b.a1 = -(p1 + p3); b.a2 = p1 * p3;
        const double g = 1.0 / std::abs (b.response (fs, 1000.0));
        b.b0 *= g; b.b1 *= g; b.b2 *= g;
        return b;
    }

    // Exact inverse of a playback biquad (numerator and denominator swapped).
    static Biquad inverse (const Biquad& p)
    {
        Biquad r;
        r.b0 = 1.0 / p.b0; r.b1 = p.a1 / p.b0; r.b2 = p.a2 / p.b0;
        r.a1 = p.b1 / p.b0; r.a2 = p.b2 / p.b0;
        return r;
    }

    // Max dB error of the digital playback filter against the analog curve.
    static double maxErrorDb (double fs, double z0)
    {
        const auto b = playback (fs, z0);
        const double fTop = std::min (20000.0, 0.45 * fs);
        double worst = 0;
        for (int i = 0; i <= 60; ++i)
        {
            const double f = 20.0 * std::pow (fTop / 20.0, i / 60.0);
            const double e = gainToDb (std::abs (b.response (fs, f))) - gainToDb (analogPlayback (f));
            worst = std::max (worst, std::abs (e));
        }
        return worst;
    }

    // Fit z0 in (-0.95, 0.95): coarse grid then golden-section refinement.
    static double fitZ0 (double fs)
    {
        double best = 0, bestErr = 1e9;
        for (int i = 0; i <= 190; ++i)
        {
            const double z = -0.95 + i * 0.01;
            const double e = maxErrorDb (fs, z);
            if (e < bestErr) { bestErr = e; best = z; }
        }
        double lo = std::max (-0.95, best - 0.01), hi = std::min (0.95, best + 0.01);
        const double gr = (std::sqrt (5.0) - 1) / 2;
        for (int it = 0; it < 40; ++it)
        {
            const double a = hi - gr * (hi - lo), b = lo + gr * (hi - lo);
            if (maxErrorDb (fs, a) < maxErrorDb (fs, b)) hi = b; else lo = a;
        }
        return 0.5 * (lo + hi);
    }
};

// Stereo-agnostic single channel RIAA pair with an accuracy control.
class RiaaPair
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        z0 = RiaaDesign::fitZ0 (fs);
        const auto perfect = RiaaDesign::playback (fs, z0);
        cut = RiaaDesign::inverse (perfect);
        play = perfect;
        lastErr = -1;
        setError (0.0);
        reset();
    }
    void reset() { cut.reset(); play.reset(); subsonic.reset(); }

    // err: 0 = perfect, 1 = cheap preamp (Character mode passes up to 2).
    void setError (double err)
    {
        if (std::abs (err - lastErr) < 1e-4) return;
        lastErr = err;
        const auto b = RiaaDesign::playback (fs, z0,
                                             RiaaDesign::t1 * (1.0 + 0.16 * err),
                                             RiaaDesign::t2 * (1.0 - 0.12 * err),
                                             RiaaDesign::t3 * (1.0 + 0.22 * err));
        // keep the filter state, swap coefficients only
        play.b0 = b.b0; play.b1 = b.b1; play.b2 = b.b2; play.a1 = b.a1; play.a2 = b.a2;
        // subsonic: 3 Hz when perfect (inaudible, < 0.01 dB at 20 Hz), up to ~35 Hz when cheap
        subsonic.highpass (fs, 3.0 + 32.0 * std::min (1.0, err), 0.7071);
    }

    inline double record (double x) { return cut.process (x); }
    inline double playback (double x) { return subsonic.process (play.process (x)); }

    double fittedZ0() const { return z0; }
    const Biquad& recordFilter() const { return cut; }
    const Biquad& playbackFilter() const { return play; }
    const Biquad& subsonicFilter() const { return subsonic; }

private:
    double fs = 48000, z0 = 0, lastErr = -1;
    Biquad cut, play, subsonic;
};
} // namespace wp
