#pragma once

#include <array>
#include <vector>

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
    Cascade leakAtoB, leakBtoA;
    Smoothed drive, mix, levelA, levelB, output;
    std::array<double, 4> bufA {}, bufB {};

    void applyRoutingDelays (bool immediate) noexcept;
};

} // namespace dfd
