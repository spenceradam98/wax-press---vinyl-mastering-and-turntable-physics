#include "Engine.h"
#include <cstring>

namespace wp
{
// Pre-emphasised groove velocity the cutter and stylus handle cleanly (1 = 0 dBFS in). Loud treble
// is boosted up to ~+20 dB by pre-emphasis, so the groove needs this much room before it bends.
constexpr double kGrooveHeadroom = 3.0;

int paramIndex (const char* id)
{
    const auto* d = paramDefs();
    for (int i = 0; i < NumParams; ++i)
        if (std::strcmp (d[i].id, id) == 0) return i;
    return -1;
}

double tracingAmount (double needlePos, int speedIndex, int stylus, double forceGrams)
{
    // Tracing distortion grows as the recorded wavelength (groove speed / frequency) shrinks
    // towards the stylus tip radius. Groove speed is 2 pi r rpm / 60, so at a fixed frequency
    // the error grows roughly with (v_ref / v)^2.
    const double pos = clampd (needlePos, 0, 1);
    const double r = kOuterRadiusMm - pos * (kOuterRadiusMm - kInnerRadiusMm);
    const double v = kTwoPi * r * rpmFor (speedIndex) / 60.0;
    const double vRef = kTwoPi * kOuterRadiusMm * rpmFor (0) / 60.0;
    const double ratio = vRef / v;
    static const double stylusFactor[] = { 1.0, 0.45, 1.8 };   // conical, elliptical (finer tip), worn
    const double sf = stylusFactor[std::clamp (stylus, 0, 2)];
    const double mistrack = clampd ((1.6 - forceGrams) / 1.1, 0, 1);   // too light: the stylus skates off the walls
    return clampd (sf * 0.1 * ratio * ratio + 0.5 * mistrack, 0, 1.5);
}

Engine::Engine()
{
    const auto* d = paramDefs();
    for (int i = 0; i < NumParams; ++i) values[(size_t) i] = d[i].def;
}

void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate > 1000 ? sampleRate : 48000;
    baseDelay = std::max (8, (int) std::lround (0.004 * fs));
    for (auto& c : ch)
    {
        c.riaa.prepare (fs);
        c.hfSplit.lowpass (fs, 5000.0, 0.7071);   // high band = x - lowpass, so the bands sum back exactly
        c.traceLp1.set (fs, 2500.0); c.traceLp2.set (fs, 2500.0);
        c.xtLp.set (fs, 1000.0);
        c.dc.set (fs, 2.0); c.evenHp.highpass (fs, 1500.0, 0.7071);
        c.warpDelay.prepare (baseDelay * 2 + 16);
        c.dryDelay.prepare (baseDelay * 2 + 16);
    }
    for (auto& h : hissHp) h.highpass (fs, 500.0, 0.7071);
    for (auto& p : popLp) p.set (fs, 2500.0);
    lowMeterS[0].lowpass (fs, 150.0, 0.7071);
    lowMeterM[0].lowpass (fs, 150.0, 0.7071);
    popDecay = std::exp (-1.0 / (0.0007 * fs));
    hfRelease = std::exp (-1.0 / (0.04 * fs));
    rumbleDrift.set (fs, 18.0, 0x5A5A5Au);
    rumbleDrift2.set (fs, 18.0, 0xA5A5A5u);
    warpDrift.set (fs, 0.4, 0x13579Bu);
    for (auto* s : { &monoAmt, &cutGain, &mixS, &outS, &warpS }) s->set (fs, 0.02);
    lastMonoHz = -1;
    reset();
}

void Engine::reset()
{
    for (auto& c : ch)
    {
        c.riaa.reset(); c.hfSplit.reset(); c.stylusLp.reset(); c.cartPeak.reset();
        c.traceLp1.reset(); c.traceLp2.reset(); c.xtLp.reset(); c.dc.reset(); c.evenHp.reset();
        c.warpDelay.reset(); c.dryDelay.reset(); c.slewPrev = 0;
    }
    monoXoverM.reset(); monoXoverS.reset();
    for (auto& b : lowMeterS) b.reset();
    for (auto& b : lowMeterM) b.reset();
    for (auto& h : hissHp) h.reset();
    for (auto& p : popLp) p.reset();
    for (auto& r : res) { r.svf.reset(); r.pending = 0; }
    for (auto& l : locked) l.left = 0;
    noiseRng = Rng (0x1234567u); crackleRng = Rng (0x7654321u); popRng = Rng (0x2468ACEu);
    hfEnv = 0; hfGain = 1; popEnv = 0; angle = prevAngle = 0;
    updateBlock (0);
    monoAmt.reset (monoAmt.target); cutGain.reset (cutGain.target); mixS.reset (mixS.target);
    outS.reset (outS.target); warpS.reset (warpS.target);
    meterIn = meterOut = meterLat = meterVert = meterVertLow = meterLatLow = meterDist = 0;
}

void Engine::updateBlock (int numSamples)
{
    const auto v = [this] (P p) { return (double) values[(size_t) p]; };
    const bool character = v (Mode) > 0.5;
    const int speed = (int) std::lround (v (Speed));
    const int stylus = (int) std::lround (v (Stylus));
    rpm = rpmFor (speed);

    // ---- needle position (Auto travel follows the host song position over one side)
    double pos = v (Needle);
    if (v (AutoTravel) > 0.5)
        pos += std::max (0.0, hostSeconds + (hostPlaying ? numSamples / fs : 0.0)) / (v (SideLength) * 60.0);
    needleNow = clampd (pos, 0, 1);

    // ---- tracing
    kNow = wp::tracingAmount (needleNow, speed, stylus, v (Tracking));
    traceDrive = 1.0 + (character ? 16.0 : 3.0) * kNow;
    traceEven = (character ? 2.5 : 0.5) * kNow;
    slewStep = kTwoPi * (45000.0 / (1.0 + (character ? 5.0 : 1.5) * kNow)) / fs;
    lacquer = character ? 0.35 : 0.0;

    // ---- cut
    const double monoHz = v (MonoBass);
    monoAmt.target = monoHz > kMonoBassOff ? 1.0 : 0.0;
    if (lastMonoHz < 0 || (monoHz > kMonoBassOff && std::abs (monoHz - lastMonoHz) > 0.25))
    {
        const double f = std::max (60.0, monoHz);
        monoXoverM.set (fs, f); monoXoverS.set (fs, f);
        lastMonoHz = monoHz;
    }
    cutGain.target = dbToGain (v (CutLevel));
    const double hf = v (HfLimit);
    hfThreshold = hf > 0.001 ? dbToGain (12.0 - 24.0 * hf) : 1e9;

    // ---- playback
    static const double stylusHz[] = { 16000.0, 21000.0, 9000.0 }, stylusQ[] = { 0.6, 0.7071, 0.55 };
    double lpHz = stylusHz[std::clamp (stylus, 0, 2)];
    if (v (Tracking) > 3.0) lpHz *= 1.0 - 0.15 * (v (Tracking) - 3.0);   // heavy tracking wears the groove walls
    const double cr = v (CartReso);
    for (auto& c : ch)
    {
        c.stylusLp.lowpass (fs, lpHz, stylusQ[std::clamp (stylus, 0, 2)]);
        c.cartPeak.peak (fs, 18000.0 - 7000.0 * cr, 0.8 + 2.2 * cr, 7.0 * cr);
        c.riaa.setError (v (Riaa) * (character ? 2.0 : 1.0));
    }
    xtAmount = 0.2 * v (Crosstalk);

    // ---- surface (levels in groove units: the playback chain and the cut level scale them)
    const double speedScale = std::sqrt (rpm / 33.333);
    hissLevel = 0.012 * v (Hiss) * v (Hiss) * speedScale + 0.004 * v (Hiss);
    crackleProb = v (Crackle) > 0 ? (3.0 + 900.0 * v (Dust) * v (Dust)) * speedScale / fs : 0.0;
    crackleAmp = 0.9 * v (Crackle);
    popProb = v (Pops) > 0 ? (0.04 + 1.4 * v (Pops) * v (Pops)) / fs : 0.0;
    popAmp = 0.5 * v (Pops);
    rumbleLevel = 0.012 * v (Rumble);
    const double rot = rpm / 60.0;
    warpDepth = (character ? 0.012 : 0.006) / (kTwoPi * rot) * fs;   // samples of delay swing per unit warp
    warpDepth = std::min (warpDepth, (double) baseDelay - 3.0);

    mixS.target = v (Mix);
    outS.target = dbToGain (v (Output));
    warpS.target = v (Warp);
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    updateBlock (n);
    const double rotInc = rpm / 60.0 / fs;
    double hfMin = 1.0, distAcc = 0, sigAcc = 0;
    double pkIn = 0, pkOut = 0, pkLat = 0, pkVert = 0, pkVertLow = 0, pkLatLow = 0;

    for (int i = 0; i < n; ++i)
    {
        const double l = inL[i], r = inR[i];
        pkIn = std::max (pkIn, std::max (std::abs (l), std::abs (r)));

        // ---------------- cut: mono bass (M/S, side below the crossover removed)
        const double m = 0.5 * (l + r), s = 0.5 * (l - r);
        const double mAp = monoXoverM.allpass (m);
        const double sAp = monoXoverS.allpass (s);
        const double sHp = monoXoverS.high (s);
        const double a = monoAmt.next();
        const double sCut = sAp + a * (sHp - sAp);

        ch[0].dryDelay.push (mAp + sAp);
        ch[1].dryDelay.push (mAp - sAp);

        pkLat = std::max (pkLat, std::abs (mAp));
        pkVert = std::max (pkVert, std::abs (sCut));
        pkVertLow = std::max (pkVertLow, std::abs (lowMeterS[0].process (sCut)));
        pkLatLow = std::max (pkLatLow, std::abs (lowMeterM[0].process (mAp)));

        const double gl = cutGain.next();
        double x[2] = { (mAp + sCut) * gl, (mAp - sCut) * gl };

        // ---------------- RIAA pre-emphasis, then the HF limiter on the boosted top
        double hi[2];
        for (int c = 0; c < 2; ++c)
        {
            x[c] = ch[c].riaa.record (x[c]);
            hi[c] = x[c] - ch[c].hfSplit.process (x[c]);
        }
        const double env = std::max (std::abs (hi[0]), std::abs (hi[1]));
        hfEnv = env > hfEnv ? env : env + hfRelease * (hfEnv - env);
        hfGain = hfEnv > hfThreshold ? hfThreshold / hfEnv : 1.0;
        hfMin = std::min (hfMin, hfGain);
        for (int c = 0; c < 2; ++c) x[c] = (x[c] - hi[c]) + hi[c] * hfGain;

        // ---------------- groove: lacquer, tracing error, slew limit
        for (int c = 0; c < 2; ++c)
        {
            auto& C = ch[c];
            double y = x[c];
            if (lacquer > 0) y += lacquer * (2.0 * std::tanh (0.5 * y) - y);
            const double lo = C.traceLp2.process (C.traceLp1.process (y));
            const double h = y - lo;
            // odd: the stylus rounds off short wavelengths; even: it rides up one groove wall more
            // than the other. The even term is high-passed so it makes no DC or low thumps.
            const double odd = kGrooveHeadroom * std::tanh (traceDrive * h / kGrooveHeadroom) / traceDrive;
            const double even = traceEven * C.evenHp.process (h * h);
            y = lo + odd + even;
            const double maxStep = slewStep;   // max groove velocity per sample (full scale = 1)
            y = C.slewPrev + clampd (y - C.slewPrev, -maxStep, maxStep);
            C.slewPrev = y;
            distAcc += std::abs (y - x[c]);
            sigAcc += std::abs (x[c]);
            x[c] = y;
        }

        // ---------------- surface noise (in the groove)
        double nL = 0, nR = 0;
        if (hissLevel > 0)
        {
            const double common = noiseRng.bi();
            nL += hissHp[0].process (0.7 * common + 0.5 * noiseRng.bi()) * hissLevel;
            nR += hissHp[1].process (0.7 * common + 0.5 * noiseRng.bi()) * hissLevel;
        }
        if (crackleProb > 0 && crackleRng.uni() < crackleProb)
        {
            auto& R = res[nextRes]; nextRes = (nextRes + 1) % kResonators;
            R.svf.set (fs, 1500.0 * std::pow (6.0, crackleRng.uni()), 1.2 + 3.0 * crackleRng.uni());
            const double u = crackleRng.uni();
            R.pending = crackleAmp * (0.12 + 0.88 * u * u * u) * (crackleRng.uni() < 0.5 ? -1.0 : 1.0);
            R.panL = 0.35 + 0.65 * crackleRng.uni(); R.panR = 0.35 + 0.65 * crackleRng.uni();
        }
        for (auto& R : res)
        {
            const double o = R.svf.bandpass (R.pending) * R.svf.k * 2.0;
            R.pending = 0;
            nL += o * R.panL; nR += o * R.panR;
        }

        // platter angle, pops (some are scratches that repeat every revolution)
        prevAngle = angle;
        angle += rotInc;
        const bool wrapped = angle >= 1.0;
        if (wrapped) angle -= 1.0;
        if (popProb > 0 && popRng.uni() < popProb)
        {
            popEnv = popAmp * (0.3 + 0.7 * popRng.uni());
            popSign = popRng.uni() < 0.5 ? -1.0 : 1.0;
            popPanL = 0.5 + 0.5 * popRng.uni(); popPanR = 0.5 + 0.5 * popRng.uni();
            if (popRng.uni() < 0.35)
                for (auto& L : locked)
                    if (L.left <= 0) { L = { angle, popEnv, 2 + (int) (popRng.uni() * 10), popPanL, popPanR }; break; }
        }
        for (auto& L : locked)
        {
            if (L.left <= 0) continue;
            const bool crossed = wrapped ? (L.phase > prevAngle || L.phase <= angle) : (L.phase > prevAngle && L.phase <= angle);
            if (crossed)
            {
                popEnv = L.amp * (0.75 + 0.25 * popRng.uni());
                popSign = 1.0; popPanL = L.panL; popPanR = L.panR;
                --L.left;
            }
        }
        if (popEnv > 1e-6)
        {
            const double raw = popEnv * popSign * (0.7 + 0.3 * popRng.bi());
            nL += popLp[0].process (raw * popPanL) * 3.0;
            nR += popLp[1].process (raw * popPanR) * 3.0;
            popEnv *= popDecay;
        }
        else { nL += popLp[0].process (0.0) * 3.0; nR += popLp[1].process (0.0) * 3.0; }
        x[0] += nL; x[1] += nR;

        // ---------------- playback: stylus, cartridge, crosstalk, RIAA de-emphasis
        double xt[2];
        for (int c = 0; c < 2; ++c)
        {
            auto& C = ch[c];
            x[c] = C.cartPeak.process (C.stylusLp.process (x[c]));
            const double lp = C.xtLp.process (x[c]);
            xt[c] = 0.35 * x[c] + 0.65 * (x[c] - lp);   // crosstalk rises with frequency
        }
        const double xl = x[0] + xtAmount * xt[1], xr = x[1] + xtAmount * xt[0];
        x[0] = xl; x[1] = xr;

        const double invGl = 1.0 / std::max (1e-6, gl);
        const double rumA = rumbleLevel > 0 ? rumbleLevel * rumbleDrift.process() : 0.0;
        const double rumB = rumbleLevel > 0 ? rumbleLevel * rumbleDrift2.process() : 0.0;
        const double w = warpS.next();
        const double phase = kTwoPi * angle;
        const double wob = w > 1e-5 ? std::sin (phase) * 0.85 + 0.15 * warpDrift.process() : 0.0;
        const double delay = (double) baseDelay + clampd (w * warpDepth * wob, -(double) baseDelay + 3.0, (double) baseDelay - 3.0);
        const double ampMod = 1.0 + 0.04 * w * std::sin (phase + 0.6);
        const double mix = mixS.next(), og = outS.next();

        for (int c = 0; c < 2; ++c)
        {
            auto& C = ch[c];
            double y = C.riaa.playback (x[c]) * invGl;
            y = C.dc.process (y);
            y += c == 0 ? rumA : 0.7 * rumA + 0.5 * rumB;   // rumble is partly vertical
            C.warpDelay.push (y);
            const double wet = (w > 1e-5 ? C.warpDelay.read (delay) : C.warpDelay.readInt (baseDelay)) * ampMod;
            const double dry = C.dryDelay.readInt (baseDelay);
            const double o = (dry + mix * (wet - dry)) * og;
            (c == 0 ? outL : outR)[i] = (float) o;
            pkOut = std::max (pkOut, std::abs (o));
        }
    }

    // ---------------- meters
    auto fall = [] (double& m, double pk, double k) { m = pk > m ? pk : m * k; };
    fall (meterIn, pkIn, 0.92); fall (meterOut, pkOut, 0.92);
    fall (meterLat, pkLat, 0.9); fall (meterVert, pkVert, 0.9);
    fall (meterVertLow, pkVertLow, 0.95); fall (meterLatLow, pkLatLow, 0.95);
    const double dist = sigAcc > 1e-9 ? std::min (1.0, 4.0 * distAcc / sigAcc) : 0.0;
    meterDist = meterDist * 0.8 + 0.2 * dist;
    inLevel = (float) meterIn; outLevel = (float) meterOut;
    lateral = (float) meterLat; vertical = (float) meterVert; verticalLow = (float) meterVertLow;
    hfGainReduction = (float) -gainToDb (hfMin, -40.0);
    distortion = (float) meterDist;
    needle = (float) needleNow;
    rotation = (float) angle;
    cutterWarning = meterVertLow > 0.1 && meterVertLow > 0.7 * meterLatLow;   // -20 dBFS of out-of-phase bass
}
} // namespace wp
