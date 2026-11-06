#include "Engine.h"

#include <cmath>

namespace dfd
{
namespace
{
    double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
} // namespace

void Engine::prepare (double sampleRate, const EngineSetup& setup, const EngineParams& initial)
{
    fs = sampleRate;
    osFactor = setup.oversampling >= 4 ? 4 : (setup.oversampling >= 2 ? 2 : 1);
    params = initial;

    for (auto* o : { &upA, &upB, &downA, &downB })
        o->prepare (osFactor);
    const double osLatency = upA.latency();
    latency = (int) std::ceil (osLatency - 1e-9);
    extraSeconds = (latency - osLatency) / fs;

    const double fsOs = fs * osFactor;
    LineSetup a { kTauA, setup.temperatureC, setup.trimA, setup.echo, setup.seed, &generated::kWallLossA };
    LineSetup b { kTauB, setup.temperatureC, setup.trimB, setup.echo, setup.seed + 1, &generated::kWallLossB };
    lineA.prepare (fsOs, a, initial.color);
    lineB.prepare (fsOs, b, initial.color);
    applyRoutingDelays (true);

    // Crosstalk (feed B, read A): -45 dB at 1 kHz rising 20 dB/decade, from undelayed electrical coupling.
    const AnalogZpk leak { { { 0.0, 0.0 } }, { { -kTwoPi * 40000.0, 0.0 } } };
    for (auto* c : { &leakAtoB, &leakBtoA })
    {
        c->setSections (bilinearSections (leak, fs, std::min (10000.0, 0.4 * fs)));
        c->scale (dbToGain (-45.0));
    }

    for (auto& d : dryDelay)
        d.assign ((size_t) latency + 1, 0.0);

    const double coeff = 1.0 - std::exp (-1.0 / (0.02 * fs));
    for (auto* s : { &drive, &mix, &levelA, &levelB, &output })
        s->coeff = coeff;
    drive.snap (dbToGain (initial.driveDb));
    mix.snap (initial.mix);
    levelA.snap (dbToGain (initial.levelADb));
    levelB.snap (dbToGain (initial.levelBDb));
    output.snap (dbToGain (initial.outputDb));
    reset();
}

void Engine::clearWet() noexcept
{
    lineA.reset();
    lineB.reset();
    for (auto* o : { &upA, &upB, &downA, &downB })
        o->reset();
    leakAtoB.reset();
    leakBtoA.reset();
}

void Engine::reset()
{
    clearWet();
    wetCleared = false;
    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0);
    dryPos = 0;
}

void Engine::applyRoutingDelays (bool immediate) noexcept
{
    lineA.setExtraDelay (extraSeconds, immediate);
    lineB.setExtraDelay (params.routing == Routing::Cascade ? 0.0 : extraSeconds, immediate);
}

void Engine::setParams (const EngineParams& p) noexcept
{
    const bool routingChanged = p.routing != params.routing;
    if (p.routing == Routing::Cascade && routingChanged)
        lineB.reset();
    params = p;
    lineA.setColor (p.color);
    lineB.setColor (p.color);
    if (routingChanged)
        applyRoutingDelays (false);
    drive.set (dbToGain (p.driveDb));
    mix.set (p.mix);
    levelA.set (dbToGain (p.levelADb));
    levelB.set (dbToGain (p.levelBDb));
    output.set (dbToGain (p.outputDb));
}

void Engine::processSample (double inL, double inR, double& outL, double& outR) noexcept
{
    const LineSwitches sw { params.nonlinear, params.noise };
    const double g = drive.next();

    double inA = inL, inB = inR;
    if (params.routing != Routing::Dual)
        inA = inB = 0.5 * (inL + inR);

    std::array<double, 4> osA {}, osB {};
    upA.up (inA, osA.data());
    if (params.routing == Routing::Cascade)
    {
        for (int i = 0; i < osFactor; ++i)
            bufA[(size_t) i] = lineB.process (lineA.process (osA[(size_t) i], g, sw), g, sw);
        outL = outR = downA.down (bufA.data());
    }
    else
    {
        upB.up (inB, osB.data());
        for (int i = 0; i < osFactor; ++i)
        {
            bufA[(size_t) i] = lineA.process (osA[(size_t) i], g, sw);
            bufB[(size_t) i] = lineB.process (osB[(size_t) i], g, sw);
        }
        outL = downA.down (bufA.data());
        outR = downB.down (bufB.data());
    }

    // Dry path, delayed to match the wet path's oversampling latency.
    double dryR;
    const double dryL = pushDry (inL, inR, dryR);
    const double dryA = params.routing == Routing::Dual ? dryL : 0.5 * (dryL + dryR);
    const double dryB = params.routing == Routing::Dual ? dryR : dryA;

    if (params.crosstalk && params.routing != Routing::Cascade)
    {
        outL += leakBtoA.process (dryB);
        outR += leakAtoB.process (dryA);
    }

    const double m = mix.next();
    const double gA = levelA.next(), gB = levelB.next(), out = output.next();
    if (params.routing == Routing::Cascade)
    {
        outL *= gA;
        outR *= gA;
    }
    else
    {
        outL *= gA;
        outR *= gB;
    }
    outL = out * ((1.0 - m) * dryL + m * outL);
    outR = out * ((1.0 - m) * dryR + m * outR);
}

double Engine::pushDry (double inL, double inR, double& dryR) noexcept
{
    auto& dl = dryDelay[0];
    auto& dr = dryDelay[1];
    dl[dryPos] = inL;
    dr[dryPos] = inR;
    dryPos = (dryPos + 1) % dl.size(); // oldest sample = latency samples ago
    dryR = dr[dryPos];
    return dl[dryPos];
}

void Engine::processBypassed (float* left, float* right, int numSamples) noexcept
{
    if (! wetCleared)
    {
        clearWet();
        wetCleared = true;
    }
    for (int i = 0; i < numSamples; ++i)
    {
        const double inL = left[i];
        const double inR = right != nullptr ? right[i] : inL;
        double dryR;
        left[i] = (float) pushDry (inL, inR, dryR);
        if (right != nullptr && right != left)
            right[i] = (float) dryR;
    }
}

void Engine::process (float* left, float* right, int numSamples) noexcept
{
    wetCleared = false;
    for (int i = 0; i < numSamples; ++i)
    {
        const double inL = left[i];
        const double inR = right != nullptr ? right[i] : inL;
        double outL, outR;
        processSample (inL, inR, outL, outR);
        left[i] = (float) outL;
        if (right != nullptr && right != left)
            right[i] = (float) outR;
    }
}

} // namespace dfd
