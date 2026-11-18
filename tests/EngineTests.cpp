// Engine tests: parameter table and presets, RIAA round trip and curve, mono bass, transparency
// and latency, inner-groove distortion vs needle position, HF limiter, Auto travel, the cutter
// warning, every preset stable at several sample rates, and CPU headroom.
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>

using namespace wp;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void applyAll (Engine& e, const std::array<float, NumParams>& v) { for (int i = 0; i < NumParams; ++i) e.setParam (i, v[(size_t) i]); }

static const Preset& presetNamed (const char* name)
{
    for (const auto& p : factoryPresets()) if (p.name == name) return p;
    std::printf ("missing preset %s\n", name);
    std::exit (1);
}

// "Mastering Check" with the mono bass and HF limiter off too: should be transparent.
static std::array<float, NumParams> transparent()
{
    auto v = presetValues (presetNamed ("Mastering Check"));
    v[MonoBass] = 40; v[HfLimit] = 0; v[CutLevel] = 0; v[Mix] = 1; v[Output] = 0;
    return v;
}

// Run a stereo generator through the engine; returns output after `skip` seconds.
template <typename Gen>
static void render (Engine& e, double fs, double seconds, Gen gen, std::vector<double>& outL, std::vector<double>& outR, int block = 256)
{
    const int total = (int) (seconds * fs);
    std::vector<float> iL ((size_t) block), iR ((size_t) block), oL ((size_t) block), oR ((size_t) block);
    outL.clear(); outR.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i) { double l, r; gen (pos + i, l, r); iL[(size_t) i] = (float) l; iR[(size_t) i] = (float) r; }
        e.process (iL.data(), iR.data(), oL.data(), oR.data(), n);
        for (int i = 0; i < n; ++i) { outL.push_back (oL[(size_t) i]); outR.push_back (oR[(size_t) i]); }
    }
}

static double rms (const std::vector<double>& v, size_t from)
{
    double s = 0; size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) { s += v[i] * v[i]; ++n; }
    return n ? std::sqrt (s / (double) n) : 0;
}

// Residual after removing the best-fit sinusoid at f (THD+N style), relative to the fundamental.
static double residualRatio (const std::vector<double>& v, size_t from, double f, double fs)
{
    double sc = 0, ss = 0, dc = 0; size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) { const double ph = kTwoPi * f * (double) i / fs; sc += v[i] * std::cos (ph); ss += v[i] * std::sin (ph); dc += v[i]; ++n; }
    sc *= 2.0 / (double) n; ss *= 2.0 / (double) n; dc /= (double) n;
    double res = 0, fund = 0;
    for (size_t i = from; i < v.size(); ++i)
    {
        const double ph = kTwoPi * f * (double) i / fs;
        const double fit = sc * std::cos (ph) + ss * std::sin (ph) + dc;
        res += (v[i] - fit) * (v[i] - fit); fund += fit * fit;
    }
    return std::sqrt (res / std::max (1e-30, fund));
}

int main()
{
    // ------------------------------------------------ parameter table
    {
        std::set<std::string> ids;
        const auto* d = paramDefs();
        for (int i = 0; i < NumParams; ++i)
        {
            CHECK (ids.insert (d[i].id).second, "duplicate id %s", d[i].id);
            CHECK (d[i].def >= d[i].min && d[i].def <= d[i].max, "default out of range for %s", d[i].id);
            CHECK (paramIndex (d[i].id) == i, "paramIndex %s", d[i].id);
        }
    }

    // ------------------------------------------------ presets
    {
        const auto& ps = factoryPresets();
        CHECK (ps.size() >= 18 && ps.size() <= 30, "about 20 presets, got %d", (int) ps.size());
        std::set<std::string> names;
        for (const auto& p : ps)
        {
            CHECK (names.insert (p.name).second, "duplicate preset %s", p.name.c_str());
            CHECK (std::find (presetBanks().begin(), presetBanks().end(), p.bank) != presetBanks().end(), "unknown bank %s", p.bank.c_str());
            for (const auto& kv : p.values) CHECK (paramIndex (kv.first.c_str()) >= 0, "preset %s: unknown id %s", p.name.c_str(), kv.first.c_str());
        }
        for (const auto& b : presetBanks())
            CHECK (std::any_of (ps.begin(), ps.end(), [&] (const Preset& p) { return p.bank == b; }), "empty bank %s", b.c_str());
    }

    // ------------------------------------------------ RIAA: round trip flat within 0.1 dB at perfect accuracy
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        RiaaPair r; r.prepare (fs); r.setError (0.0);
        const double fTop = std::min (20000.0, 0.45 * fs);
        double worst = 0;
        for (int i = 0; i <= 80; ++i)
        {
            const double f = 20.0 * std::pow (fTop / 20.0, i / 80.0);
            const auto h = r.recordFilter().response (fs, f) * r.playbackFilter().response (fs, f) * r.subsonicFilter().response (fs, f);
            worst = std::max (worst, std::abs (gainToDb (std::abs (h))));
        }
        CHECK (worst < 0.1, "RIAA round trip at %.0f Hz sample rate: %.3f dB", fs, worst);

        // and in the time domain, sines through the actual filters
        for (double f : { 20.0, 100.0, 1000.0, 6000.0, std::min (18000.0, 0.4 * fs) })
        {
            RiaaPair rt; rt.prepare (fs); rt.setError (0.0);
            const int n = (int) (fs * 1.0), skip = (int) (fs * 0.5);
            double si = 0, so = 0;
            for (int k = 0; k < n; ++k)
            {
                const double x = 0.25 * std::sin (kTwoPi * f * k / fs);
                const double y = rt.playback (rt.record (x));
                if (k >= skip) { si += x * x; so += y * y; }
            }
            const double db = 10.0 * std::log10 (so / si);
            CHECK (std::abs (db) < 0.1, "RIAA round trip sine %.0f Hz at fs %.0f: %.3f dB", f, fs, db);
        }

        // playback curve against the standard (analog) values
        const double tol = fs < 60000 ? 0.4 : 0.1;
        CHECK (RiaaDesign::maxErrorDb (fs, r.fittedZ0()) < tol, "RIAA curve error %.3f dB at fs %.0f", RiaaDesign::maxErrorDb (fs, r.fittedZ0()), fs);
        const struct { double f, db; } ref[] = { { 20, 19.27 }, { 100, 13.09 }, { 1000, 0.0 }, { 10000, -13.73 } };
        for (const auto& p : ref)
        {
            const double got = gainToDb (std::abs (r.playbackFilter().response (fs, p.f)));
            CHECK (std::abs (got - p.db) < tol, "RIAA playback %.0f Hz: %.2f dB, expected %.2f (fs %.0f)", p.f, got, p.db, fs);
        }
        // detuned ("cheap preamp") playback must differ audibly from perfect
        RiaaPair cheap; cheap.prepare (fs); cheap.setError (1.0);
        double diff = 0;
        for (double f : { 50.0, 200.0, 3000.0, 10000.0 })
            diff = std::max (diff, std::abs (gainToDb (std::abs (cheap.playbackFilter().response (fs, f) * r.recordFilter().response (fs, f)))));
        CHECK (diff > 0.5 && diff < 6.0, "cheap preamp tilt %.2f dB", diff);
    }

    const double fs = 48000;

    // ------------------------------------------------ transparency and latency
    {
        Engine e; applyAll (e, transparent()); e.prepare (fs, 256);
        std::vector<double> L, R;
        render (e, fs, 1.0, [] (int k, double& l, double& r) { l = 0.1 * std::sin (kTwoPi * 1000.0 * k / 48000.0); r = 0.1 * std::sin (kTwoPi * 440.0 * k / 48000.0); }, L, R);
        const double gl = gainToDb (rms (L, 24000) / (0.1 / std::sqrt (2.0))), gr = gainToDb (rms (R, 24000) / (0.1 / std::sqrt (2.0)));
        CHECK (std::abs (gl) < 0.1 && std::abs (gr) < 0.1, "transparent settings change level: L %.3f dB, R %.3f dB", gl, gr);
        CHECK (residualRatio (L, 24000, 1000.0, fs) < 0.003, "transparent settings distort: %.5f", residualRatio (L, 24000, 1000.0, fs));

        Engine e2; applyAll (e2, transparent()); e2.prepare (fs, 256);
        render (e2, fs, 0.1, [] (int k, double& l, double& r) { l = r = (k == 100 ? 0.5 : 0.0); }, L, R);
        size_t pk = 0; for (size_t i = 0; i < L.size(); ++i) if (std::abs (L[i]) > std::abs (L[pk])) pk = i;
        CHECK (std::abs ((int) pk - 100 - e2.latencySamples()) <= 1, "latency: impulse peak at %d, reported %d", (int) pk - 100, e2.latencySamples());

        // dry (Mix 0) is time aligned with wet: a mixed sine must not comb
        auto v = transparent(); v[Mix] = 0.5f;
        Engine e3; applyAll (e3, v); e3.prepare (fs, 256);
        render (e3, fs, 1.0, [] (int k, double& l, double& r) { l = r = 0.1 * std::sin (kTwoPi * 5000.0 * k / 48000.0); }, L, R);
        CHECK (std::abs (gainToDb (rms (L, 24000) / (0.1 / std::sqrt (2.0)))) < 0.15, "dry/wet mix combs at 5 kHz");
    }

    // ------------------------------------------------ silence in, silence out (no surface noise)
    {
        Engine e; applyAll (e, presetValues (presetNamed ("Mastering Check"))); e.prepare (fs, 256);
        std::vector<double> L, R;
        render (e, fs, 1.0, [] (int, double& l, double& r) { l = r = 0; }, L, R);
        double pk = 0; for (auto x : L) pk = std::max (pk, std::abs (x));
        CHECK (pk < 1e-6, "Mastering Check makes noise from silence: %g", pk);
    }

    // ------------------------------------------------ mono bass
    {
        auto v = transparent(); v[MonoBass] = 150;
        auto run = [&] (double f, bool sideOnly, double& outMid, double& outSide)
        {
            Engine e; applyAll (e, v); e.prepare (fs, 256);
            std::vector<double> L, R;
            render (e, fs, 1.5, [&] (int k, double& l, double& r) { const double x = 0.2 * std::sin (kTwoPi * f * k / fs); l = x; r = sideOnly ? -x : x; }, L, R);
            std::vector<double> M (L.size()), S (L.size());
            for (size_t i = 0; i < L.size(); ++i) { M[i] = 0.5 * (L[i] + R[i]); S[i] = 0.5 * (L[i] - R[i]); }
            outMid = rms (M, 48000); outSide = rms (S, 48000);
        };
        const double ref = 0.2 / std::sqrt (2.0);
        double m, s;
        // mono material passes untouched (L == R, level kept): the mono sum is correct
        for (double f : { 40.0, 150.0, 1000.0 })
        {
            run (f, false, m, s);
            CHECK (std::abs (gainToDb (m / ref)) < 0.1, "mono bass changes the mono sum at %.0f Hz: %.3f dB", f, gainToDb (m / ref));
            CHECK (s < ref * 1e-4, "mono bass creates side from mono at %.0f Hz", f);
        }
        run (40.0, true, m, s);
        CHECK (gainToDb (s / ref) < -24.0, "side at 40 Hz only %.1f dB down", gainToDb (s / ref));
        CHECK (m < ref * 1e-4, "side at 40 Hz leaks into mid");
        run (2000.0, true, m, s);
        CHECK (std::abs (gainToDb (s / ref)) < 0.5, "side at 2 kHz changed by %.2f dB", gainToDb (s / ref));
    }

    // ------------------------------------------------ inner-groove distortion grows towards the label
    {
        auto thd = [&] (float needle, int mode, int stylus)
        {
            auto v = transparent(); v[Needle] = needle; v[Mode] = (float) mode; v[Stylus] = (float) stylus;
            Engine e; applyAll (e, v); e.prepare (fs, 256);
            std::vector<double> L, R;
            render (e, fs, 1.0, [&] (int k, double& l, double& r) { l = r = 0.25 * std::sin (kTwoPi * 7000.0 * k / fs); }, L, R);
            return residualRatio (L, 24000, 7000.0, fs);
        };
        const double outer = thd (0, 0, 0), inner = thd (1, 0, 0), innerChar = thd (1, 1, 0), innerEll = thd (1, 0, 1), innerWorn = thd (1, 0, 2);
        std::printf ("tracing THD @7k: outer %.4f inner %.4f inner(char) %.4f inner(ell) %.4f inner(worn) %.4f\n", outer, inner, innerChar, innerEll, innerWorn);
        CHECK (inner > 2.0 * outer, "inner groove not more distorted (outer %.4f, inner %.4f)", outer, inner);
        CHECK (innerChar > 1.5 * inner, "Character mode not exaggerated (%.4f vs %.4f)", innerChar, inner);
        CHECK (innerEll < inner, "elliptical should trace better than conical (ell %.4f con %.4f)", innerEll, inner);
        CHECK (tracingAmount (1, 0, 2, 2) > tracingAmount (1, 0, 0, 2) && tracingAmount (1, 0, 0, 2) > tracingAmount (1, 0, 1, 2), "stylus order wrong");
        (void) innerWorn;   // the worn stylus also rolls off the top, which hides its harmonics at 7 kHz
        CHECK (tracingAmount (1, 0, 0, 2) > tracingAmount (1, 1, 0, 2) && tracingAmount (1, 1, 0, 2) > tracingAmount (1, 2, 0, 2), "faster speeds should trace better");
        CHECK (tracingAmount (0.5, 0, 0, 0.6) > tracingAmount (0.5, 0, 0, 2.0), "light tracking should mistrack");
    }

    // ------------------------------------------------ HF limiter
    {
        auto level = [&] (float hf)
        {
            auto v = transparent(); v[HfLimit] = hf;
            Engine e; applyAll (e, v); e.prepare (fs, 256);
            std::vector<double> L, R;
            render (e, fs, 0.6, [&] (int k, double& l, double& r) { l = r = 0.2 * std::sin (kTwoPi * 10000.0 * k / fs); }, L, R);
            return std::make_pair (rms (L, 9600), (double) e.hfGainReduction.load());
        };
        const auto off = level (0), on = level (1);
        CHECK (on.first < off.first * 0.7, "HF limiter does not limit (%.3f vs %.3f)", on.first, off.first);
        CHECK (on.second > 3.0 && off.second < 0.01, "HF limiter meter (on %.2f dB, off %.2f dB)", on.second, off.second);
        // and leaves the bass alone
        auto v = transparent(); v[HfLimit] = 1;
        Engine e; applyAll (e, v); e.prepare (fs, 256);
        std::vector<double> L, R;
        render (e, fs, 0.6, [&] (int k, double& l, double& r) { l = r = 0.5 * std::sin (kTwoPi * 100.0 * k / fs); }, L, R);
        CHECK (std::abs (gainToDb (rms (L, 9600) / (0.5 / std::sqrt (2.0)))) < 0.2, "HF limiter touches 100 Hz");
    }

    // ------------------------------------------------ Auto travel follows the host
    {
        auto v = transparent(); v[AutoTravel] = 1; v[SideLength] = 20; v[Needle] = 0.1f;
        Engine e; applyAll (e, v); e.prepare (fs, 256);
        std::vector<float> z (256, 0.0f), o1 (256), o2 (256);
        e.setTransport (600.0, true); e.process (z.data(), z.data(), o1.data(), o2.data(), 256);
        CHECK (std::abs (e.effectiveNeedle() - 0.6) < 0.01, "auto travel at 10 of 20 min from 0.1: %.3f", e.effectiveNeedle());
        e.setTransport (5000.0, true); e.process (z.data(), z.data(), o1.data(), o2.data(), 256);
        CHECK (e.effectiveNeedle() == 1.0, "auto travel clamps at the label");
        e.setParam (AutoTravel, 0); e.process (z.data(), z.data(), o1.data(), o2.data(), 256);
        CHECK (std::abs (e.effectiveNeedle() - 0.1) < 1e-6, "auto travel off uses the knob");
    }

    // ------------------------------------------------ cutter warning (out-of-phase bass)
    {
        auto warn = [&] (float mono)
        {
            auto v = transparent(); v[MonoBass] = mono;
            Engine e; applyAll (e, v); e.prepare (fs, 256);
            std::vector<double> L, R;
            render (e, fs, 0.5, [&] (int k, double& l, double& r) { const double x = 0.6 * std::sin (kTwoPi * 60.0 * k / fs); l = x; r = -0.3 * x; }, L, R);
            return e.cutterWarning.load();
        };
        CHECK (warn (40), "out-of-phase bass with mono bass off should warn");
        CHECK (! warn (200), "mono bass at 200 Hz should clear the warning");
    }

    // ------------------------------------------------ every preset at several rates: finite, bounded, sane noise
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto& p : factoryPresets())
        {
            Engine e; applyAll (e, presetValues (p)); e.prepare (rate, 512);
            std::vector<double> L, R;
            Rng rng (11);
            OnePoleLP lp; lp.set (rate, 4000);
            render (e, rate, 2.0, [&] (int k, double& l, double& r)
            {
                const double t = k / rate;
                const double x = (t < 1.5) ? 0.3 * std::sin (kTwoPi * 110.0 * t) * (0.6 + 0.4 * std::sin (kTwoPi * 2 * t)) + 0.1 * lp.process (rng.bi()) : 0.0;
                l = x; r = 0.8 * x;
            }, L, R, 512);
            bool finite = true; double pk = 0;
            for (size_t i = 0; i < L.size(); ++i) { finite = finite && std::isfinite (L[i]) && std::isfinite (R[i]); pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i]))); }
            CHECK (finite, "%s: non-finite output at %.0f", p.name.c_str(), rate);
            CHECK (pk < 3.0, "%s: peak %.2f at %.0f", p.name.c_str(), pk, rate);
            const double tail = rms (L, (size_t) (1.7 * rate));
            CHECK (tail < 0.1, "%s: tail after the music is loud (%.1f dB)", p.name.c_str(), gainToDb (tail));
            if (rate == 48000.0) std::printf ("  %-26s peak %6.1f dB  noise floor %6.1f dB\n", p.name.c_str(), gainToDb (pk), gainToDb (tail));
        }

    // ------------------------------------------------ CPU: worst preset, 48 kHz stereo
    {
        Engine e; applyAll (e, presetValues (presetNamed ("Basement Crate"))); e.prepare (fs, 256);
        std::vector<double> L, R;
        const auto t0 = std::chrono::steady_clock::now();
        render (e, fs, 10.0, [] (int k, double& l, double& r) { l = r = 0.2 * std::sin (k * 0.01); }, L, R);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("CPU: 10 s of audio in %.3f s (%.0fx real time)\n", sec, 10.0 / sec);
        CHECK (sec < 2.0, "too slow: %.2f s for 10 s", sec);
    }

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
