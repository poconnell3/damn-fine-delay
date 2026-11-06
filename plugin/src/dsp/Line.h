#pragma once

#include <cstdint>
#include <vector>

#include "Filters.h"
#include "GeneratedFilters.h"

namespace dfd
{
enum class Color { A, B };
enum class NoiseMode { Off, Authentic, Extra };

/** Positions of the three per-unit EQ trimmers (ohms). */
struct Trim
{
    double midOhm = 2500.0; // R4 MID RANGE, send, 0..5k
    double lfOhm = 1000.0;  // R21 L.F., receive, 0..2k
    double hfOhm = 500.0;   // R23 10KC, receive, 0..500
};

/** Settings that require re-preparing the engine. */
struct LineSetup
{
    double tauNominal = 0.016;
    double temperatureC = 20.0;
    Trim trim;
    bool echo = true;
    uint64_t seed = 1;
    const generated::WallLossFit* wallLoss = &generated::kWallLossA;
};

/** Realtime switches, read every sample. */
struct LineSwitches
{
    bool nonlinear = true;
    NoiseMode noise = NoiseMode::Authentic;
};

/** One acoustic delay line of the 920-16 running at the oversampled rate:
    drive -> input transformer -> SEND EQ -> output RC -> driver excursion limit -> transducer pair
    -> stub high-pass -> air nonlinearity -> tube (wall loss, delay, first echo at 3 tau) -> pickup noise
    -> RECEIVE EQ -> output transformer -> 1/drive. */
class Line
{
public:
    static constexpr double kNominalDbfs = -18.0;     // = +4 dBu
    static constexpr double kSnDb = 72.0;
    static constexpr double kNoiseBandwidthHz = 15700.0;
    static constexpr double kEchoReflection = 0.1 * 0.9;
    static constexpr double kCutoffHz = 11000.0;
    static constexpr int kCutoffOrder = 8;
    static constexpr double kCutoffRippleDb = 0.1;
    static constexpr double kEqWarpHz = 10400.0;

    void prepare (double fsOversampled, const LineSetup& setup, Color initialColor);
    void reset();

    void setColor (Color c) noexcept;

    /** Extra wet delay (s) on top of tau, e.g. to make the oversampling latency a whole number of samples. */
    void setExtraDelay (double seconds, bool immediate) noexcept;

    /** One oversampled sample. `drive` is the linear drive gain (output is trimmed back by 1/drive). */
    double process (double x, double drive, const LineSwitches& sw) noexcept;

    double delaySeconds() const noexcept { return tau; }
    double groupDelayCorrection (Color c) const noexcept { return c == Color::A ? iirDelayA : iirDelayB; }

private:
    struct FluxSaturator
    {
        Cascade flux, inverse;
        double sat = 12.0;
        void prepare (double fs);
        double process (double x) noexcept { return inverse.process (sat * std::tanh (flux.process (x) / sat)); }
        void reset() noexcept { flux.reset(); inverse.reset(); }
    };

    double fs = 96000.0, tau = 0.016;
    Cascade send, rc, transducer, residuals, stubHp, wallLoss, echoWallLoss, receive, excursion;
    FluxSaturator inXfmr, outXfmr;
    double makeup = 1.0, kAir = 0.0, prevSq = 0.0, limitPos = 1.0, limitNeg = 1.0, noiseSigma = 0.0;
    double iirDelayA = 0.0, iirDelayB = 0.0, extraDelay = 0.0;
    Color currentColor = Color::A;
    bool echoOn = true;

    // color crossfade (0 = B, 1 = A) and the matching delay glide
    double colorMix = 1.0, colorTarget = 1.0, colorStep = 0.0;

    std::vector<double> buffer;
    size_t writePos = 0;
    struct Tap
    {
        double delay = 0.0, target = 0.0, step = 0.0, thiranY = 0.0;
    } direct, echoTap;

    uint64_t rngState = 1;
    bool haveSpare = false;
    double spare = 0.0;

    double readThiran (Tap& t) noexcept;
    double gaussian() noexcept;
    void setTapTargets (Color c, bool immediate) noexcept;
};

} // namespace dfd
