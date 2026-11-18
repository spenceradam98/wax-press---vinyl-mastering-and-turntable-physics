// wax-press engine: cut a stereo mix to "lacquer" and play it back. Plain C++17, no JUCE,
// so it can be unit tested.
//
//   in -> mono bass (M/S, LR4) -> cut level -> RIAA pre-emphasis -> HF limiter
//      -> groove tracing (inner-groove distortion: HF shaper + slew limit)
//      -> + surface (hiss, crackle, pops: they live in the groove, so de-emphasis darkens them)
//      -> stylus / tracking -> cartridge resonance -> crosstalk -> RIAA de-emphasis (accuracy)
//      -> / cut level -> + rumble -> warp (wow at the rotation rate) -> mix -> output
#pragma once

#include "Dsp.h"
#include "Params.h"
#include "Riaa.h"
#include <array>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace wp
{
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Parameter values in their natural units (as in the ParamDef table).
    void setParam (int index, float value) { if (index >= 0 && index < NumParams) values[(size_t) index] = value; }
    float getParam (int index) const { return values[(size_t) index]; }

    // Host transport, for Auto travel. seconds = song position of the first sample of the block.
    void setTransport (double seconds, bool playing) { hostSeconds = seconds; hostPlaying = playing; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    // Constant latency (the warp delay line's centre tap), in samples.
    int latencySamples() const { return baseDelay; }

    // Needle position actually used (0 outer .. 1 inner), after Auto travel.
    double effectiveNeedle() const { return needleNow; }
    // Inner-groove distortion amount for the current settings (0..~1.5).
    double tracingAmount() const { return kNow; }

    // Metering for the UI (written on the audio thread).
    std::atomic<float> inLevel { 0 }, outLevel { 0 };
    std::atomic<float> lateral { 0 }, vertical { 0 };     // peak of mid and side as cut (after mono bass)
    std::atomic<float> verticalLow { 0 };                 // side below 150 Hz as cut
    std::atomic<float> hfGainReduction { 0 };             // dB
    std::atomic<float> distortion { 0 };                  // 0..1, tracing error relative to signal
    std::atomic<float> needle { 0 };                      // effective needle position
    std::atomic<float> rotation { 0 };                    // platter angle, 0..1 (for the UI)
    std::atomic<bool> cutterWarning { false };            // out-of-phase bass would lift the cutter

private:
    void updateBlock (int numSamples);

    double fs = 48000;
    std::array<float, NumParams> values {};
    double hostSeconds = 0; bool hostPlaying = false;

    // per channel
    struct Channel
    {
        RiaaPair riaa;
        Biquad hfSplit, stylusLp, cartPeak, evenHp;
        OnePoleLP traceLp1, traceLp2, xtLp;
        DcBlocker dc;
        DelayLine warpDelay, dryDelay;
        double slewPrev = 0;
    };
    Channel ch[2];

    LR4 monoXoverM, monoXoverS;
    Biquad lowMeterS[2], lowMeterM[2];
    double lastMonoHz = -1;

    // HF limiter
    double hfEnv = 0, hfGain = 1, hfRelease = 0.999, hfMeter = 0;

    // surface
    Rng noiseRng { 0x1234567u }, crackleRng { 0x7654321u }, popRng { 0x2468ACEu };
    Biquad hissHp[2];
    static constexpr int kResonators = 6;
    struct Resonator { Svf svf; double energyL = 0, energyR = 0; double pending = 0; double panL = 1, panR = 1; };
    Resonator res[kResonators];
    int nextRes = 0;
    struct Locked { double phase = 0; double amp = 0; int left = 0; double panL = 1, panR = 1; };
    Locked locked[4];
    OnePoleLP popLp[2];
    double popEnv = 0, popPanL = 1, popPanR = 1, popSign = 1, popDecay = 0.99;
    double hissLevel = 0, crackleProb = 0, crackleAmp = 0, popProb = 0, popAmp = 0, rumbleLevel = 0, rpm = 33.333;
    double warpDepth = 0, xtAmount = 0, hfThreshold = 1e9;
    Drift rumbleDrift, rumbleDrift2, warpDrift;
    double angle = 0, prevAngle = 0;

    // smoothing
    Smoothed monoAmt, cutGain, mixS, outS, warpS;
    int baseDelay = 192;

    // block state
    double needleNow = 0, kNow = 0;
    double traceDrive = 1, traceEven = 0, slewStep = 1, lacquer = 0;
    double meterIn = 0, meterOut = 0, meterLat = 0, meterVert = 0, meterVertLow = 0, meterLatLow = 0, meterDist = 0;
};

// ---------------------------------------------------------------- Presets
struct Preset
{
    std::string name;
    std::string bank;
    std::vector<std::pair<std::string, float>> values;   // overrides on top of the parameter defaults
};

const std::vector<Preset>& factoryPresets();
const std::vector<std::string>& presetBanks();

// Full parameter vector for a preset (defaults + overrides).
std::array<float, NumParams> presetValues (const Preset&);

// Tracing (inner-groove distortion) amount for a needle position, speed, stylus, force and mode.
double tracingAmount (double needlePos, int speedIndex, int stylus, double forceGrams);
} // namespace wp
