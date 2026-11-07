#pragma once

#include <array>
#include <vector>

#include "Humanizer.h"
#include "Line.h"
#include "Oversampler.h"

namespace dfd
{
enum class Routing { Widener, Dual, Cascade };

/** Settings that need `prepare()` again (they change latency, buffers or calibration). */
struct EngineSetup
{
    int oversampling = 2; // 1, 2 or 4
    double temperatureC = 20.0;
    Trim trimA, trimB;
    bool echo = true;
    uint64_t seed = 1;
};

/** Realtime parameters, smoothed inside the engine. */
struct EngineParams
{
    Routing routing = Routing::Widener;
    Color color = Color::A;
    double driveDb = 0.0;
    double mix = 1.0; // 0 = dry, 1 = wet
    double levelADb = 0.0, levelBDb = 0.0;
    double outputDb = 0.0;
    NoiseMode noise = NoiseMode::Authentic;

    /** Doubler mode: the dry signal and the 30 ms cascade are mixed as two mono sources, each with its own
        pan (-1 = left, +1 = right, constant power) and level. Mix and Level A/B are ignored; routing should be Cascade. */
    bool doubler = false;
    double dryPan = -0.6, wetPan = 0.6;
    double dryLevelDb = 0.0, wetLevelDb = 0.0;

    /** Humanize (doubler only): each new note of the 30 ms copy lands at its own random time within
        +-humanMs, held for the whole note, like a second take played by hand. Off keeps it exact. */
    bool human = false;
    double humanMs = 3.0;

    bool nonlinear = true;
    bool crosstalk = true;
};

/** Damn Fine Delay: two acoustic lines (A = 16 ms, B = 14 ms) with the hardware's routings.
    Base-rate stereo in/out; the lines run oversampled. Wet output is aligned to the dry path,
    and both are delayed by latencySamples(). */
class Engine
{
public:
    static constexpr double kTauA = 0.016, kTauB = 0.014;

    void prepare (double sampleRate, const EngineSetup& setup, const EngineParams& initial);
    void reset();

    /** Reported latency in base-rate samples. */
    int latencySamples() const noexcept { return latency; }

    void setParams (const EngineParams& p) noexcept;

    /** In-place stereo processing (either pointer may alias the other for mono hosts). */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Bypass: the input delayed by latencySamples(), through the same dry delay as process(), so
        switching in and out is seamless. The lines are cleared on entry and restart from silence. */
    void processBypassed (float* left, float* right, int numSamples) noexcept;

    /** Offline helpers for tests: process one base-rate sample per call. */
    void processSample (double inL, double inR, double& outL, double& outR) noexcept;

private:
    struct Smoothed
    {
        double value = 0.0, target = 0.0, coeff = 0.0;
        void set (double t) noexcept { target = t; }
        void snap (double t) noexcept { value = target = t; }
        double next() noexcept { return value += coeff * (target - value); }
    };

    double fs = 48000.0;
    int osFactor = 2;
    int latency = 0;
    EngineParams params;
    Line lineA, lineB;
    Oversampler upA, upB, downA, downB;
    double extraSeconds = 0.0; // sub-sample latency trim, added to the wet path
    std::array<std::vector<double>, 2> dryDelay;
    size_t dryPos = 0;
    bool wetCleared = false;
    Cascade leakAtoB, leakBtoA;
    Smoothed drive, mix, levelA, levelB, output;
    Smoothed doublerAmount, dryPan, wetPan, dryLevel, wetLevel;

    // Doubler: the cascade input runs through the humanizer's pre-delay, which the tubes give back
    // (retimed). Changing that is done under a short mute of the wet signal so the tubes can jump.
    Humanizer humanizer;
    bool retimed = false;
    double wetDuck = 1.0, duckStep = 0.01;
    bool monoOutput = false;
    std::array<double, 4> bufA {}, bufB {};

    void applyRoutingDelays (bool immediate) noexcept;
    void advanceRetime() noexcept;
    void clearWet() noexcept;
    double pushDry (double inL, double inR, double& dryR) noexcept;
};

} // namespace dfd
