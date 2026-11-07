#include "Line.h"

#include <cmath>

namespace rrd
{
namespace
{
    constexpr double kAirH2At1k = 0.00076; // 2nd harmonic at the air-nonlinearity point for a nominal 1 kHz sine
    const double kNominalPeak = std::pow (10.0, Line::kNominalDbfs / 20.0) * std::sqrt (2.0);

    double speedOfSound (double tempC) { return 331.3 * std::sqrt (1.0 + tempC / 273.15); }

    AnalogZpk wallLossZpk (const generated::WallLossFit& w)
    {
        AnalogZpk z;
        for (size_t i = 0; i < w.zeros.size(); ++i)
        {
            z.zeros.push_back ({ -w.zeros[i], 0.0 });
            z.poles.push_back ({ -w.poles[i], 0.0 });
        }
        return z;
    }

    AnalogZpk rationalZpk (const generated::Rational& r)
    {
        return zpkFromRational (r.num, r.den, generated::kNormRadPerSec);
    }

    double knee (double e, double limit) noexcept
    {
        constexpr double k = 0.7;
        const double a = k * limit, b = (1.0 - k) * limit;
        return e <= a ? e : a + b * std::tanh ((e - a) / b);
    }

    /** Residual faults of a well-calibrated unit (doc 06 section 4): Hz, dB, Q. */
    constexpr double kResiduals[][3] = { { 7000.0, -1.0, 6.0 }, { 9000.0, -0.6, 5.0 }, { 3000.0, 0.5, 1.5 } };
} // namespace

void Line::FluxSaturator::prepare (double rate)
{
    constexpr double fLow = 4.0, fHigh = 40.0;
    const AnalogZpk shelf { { { -kTwoPi * fHigh, 0.0 } }, { { -kTwoPi * fLow, 0.0 } } };
    flux.setSections (bilinearSections (shelf, rate, fHigh, 20000.0));
    inverse.setSections (bilinearSections (shelf.inverse(), rate, fHigh, 20000.0));
}

void Line::prepare (double fsOversampled, const LineSetup& setup, Color initialColor)
{
    fs = fsOversampled;
    tau = setup.tauNominal * speedOfSound (20.0) / speedOfSound (setup.temperatureC);
    echoOn = setup.echo;

    const auto sendNet = rationalZpk (generated::sendNetwork (setup.trim.midOhm));
    const auto receiveNet = rationalZpk (generated::receiveNetwork (setup.trim.lfOhm, std::max (setup.trim.hfOhm, 1.0)));
    const Trim factory;
    const auto sendFactory = rationalZpk (generated::sendNetwork (factory.midOhm));
    const auto receiveFactory = rationalZpk (generated::receiveNetwork (factory.lfOhm, factory.hfOhm));

    // R1 120 ohm into C3 0.1 uF || ~200 ohm driver: a first-order low-pass
    const double rcPole = 1.0 / (0.1e-6 * 120.0 * 200.0 / 320.0);
    const AnalogZpk rcZpk { {}, { { -rcPole, 0.0 } } };
    const auto wall = wallLossZpk (*setup.wallLoss);

    send.setSections (bilinearSections (sendNet, fs, kEqWarpHz));
    rc.setSections (bilinearSections (rcZpk, fs, kCutoffHz));
    receive.setSections (bilinearSections (receiveNet, fs, kEqWarpHz));
    wallLoss.setSections (bilinearSections (wall, fs, kEqWarpHz, 0.0));

    // Transducer pair: the inverse of the factory-trimmed electronics and the tube, times the band-edge
    // low-pass. Same warping as the forward filters, so at factory trims the chain cancels exactly.
    // The RC inverse is improper on its own, so it is mapped together with the low-pass (same warp as the
    // forward RC); the extra low-pass zeros at z = -1 then match the RC's, and the product is exact.
    std::vector<Biquad> xd;
    for (const auto& part : { sendFactory.inverse(), receiveFactory.inverse(), wall.inverse() })
    {
        auto s = bilinearSections (part, fs, kEqWarpHz);
        xd.insert (xd.end(), s.begin(), s.end());
    }
    auto edge = chebyshev1LowPass (kCutoffOrder, kCutoffRippleDb, kCutoffHz);
    edge.append (rcZpk.inverse());
    for (auto& s : bilinearSections (edge, fs, kCutoffHz))
        xd.push_back (s);
    transducer.setSections (std::move (xd));
    transducer.normalizeAt (1000.0, fs);

    std::vector<Biquad> res;
    for (const auto& r : kResiduals)
        for (auto& s : bilinearSections (bell (r[0], r[1], r[2]), fs, r[0]))
            res.push_back (s);
    residuals.setSections (std::move (res));
    residuals.normalizeAt (1000.0, fs);

    stubHp.setSections (bilinearSections (butterworth (2, 15.0, true), fs, 15.0));
    stubHp.normalizeAt (20000.0, fs);

    // Echo: two extra traversals of the tube relative to the direct sound.
    {
        std::vector<Biquad> e;
        for (int k = 0; k < 2; ++k)
            for (auto& s : bilinearSections (wall, fs, kEqWarpHz, 0.0))
                e.push_back (s);
        echoWallLoss.setSections (std::move (e));
    }
    makeup = 1.0 / std::abs (wallLoss.response (1000.0, fs));

    excursion.setSections (bilinearSections (butterworth (2, 200.0, false), fs, 200.0, 0.0));
    inXfmr.prepare (fs);
    outXfmr.prepare (fs);

    // Calibration: R50 strikes its cover plate at 100 Hz, +10 dBu in.
    {
        const double aIn = std::pow (10.0, (10.0 - 4.0 + kNominalDbfs) / 20.0) * std::sqrt (2.0);
        const double up = std::abs (send.response (100.0, fs) * rc.response (100.0, fs));
        limitPos = aIn * up * std::abs (excursion.response (100.0, fs));
        limitNeg = 1.5 * limitPos;
    }
    kAir = kAirH2At1k / (kNominalPeak * kTwoPi * 1000.0);

    // Pickup noise: white at the receive input, 72 dB below nominal at the output in 15.7 kHz.
    {
        constexpr int n = 4096;
        double acc = 0.0;
        const double df = kNoiseBandwidthHz / n;
        for (int i = 0; i <= n; ++i)
        {
            const double f = std::max (i * df, 0.1);
            const double w = (i == 0 || i == n) ? 0.5 : 1.0;
            acc += w * std::norm (receive.response (f, fs)) * df;
        }
        const double fraction = acc / (fs / 2.0);
        noiseSigma = std::pow (10.0, (kNominalDbfs - kSnDb) / 20.0) / std::sqrt (fraction);
    }

    // Bulk delay: the whole line must read tau at 1 kHz.
    auto chainDelay = [this] (bool withResiduals)
    {
        return groupDelayAt ([&] (double f)
                             {
                                 cplx h = send.response (f, fs) * rc.response (f, fs) * transducer.response (f, fs)
                                        * stubHp.response (f, fs) * wallLoss.response (f, fs) * receive.response (f, fs);
                                 return withResiduals ? h * residuals.response (f, fs) : h;
                             },
                             1000.0);
    };
    iirDelayA = chainDelay (true);
    iirDelayB = chainDelay (false);

    const size_t needed = (size_t) std::ceil ((3.0 * tau + 0.01) * fs) + 8;
    buffer.assign (needed, 0.0);
    rngState = 0x9E3779B97F4A7C15ull ^ (setup.seed * 0xD1B54A32D192ED03ull);
    colorMix = colorTarget = initialColor == Color::A ? 1.0 : 0.0;
    currentColor = initialColor;
    extraDelay = 0.0;
    reset();
    setTapTargets (initialColor, true);
}

void Line::reset()
{
    for (auto* c : { &send, &rc, &transducer, &residuals, &stubHp, &wallLoss, &echoWallLoss, &receive, &excursion })
        c->reset();
    inXfmr.reset();
    outXfmr.reset();
    std::fill (buffer.begin(), buffer.end(), 0.0);
    writePos = 0;
    prevSq = 0.0;
    direct.thiranY = echoTap.thiranY = 0.0;
    haveSpare = false;
}

void Line::setTapTargets (Color c, bool immediate) noexcept
{
    const double d = (tau + extraDelay - groupDelayCorrection (c)) * fs;
    const double e = d + 2.0 * tau * fs;
    constexpr double glideSeconds = 0.02;
    const double steps = std::max (1.0, glideSeconds * fs);
    for (auto [tap, target] : { std::pair { &direct, d }, std::pair { &echoTap, e } })
    {
        tap->target = target;
        if (immediate)
        {
            tap->delay = target;
            tap->step = 0.0;
        }
        else
            tap->step = (target - tap->delay) / steps;
    }
}

void Line::setColor (Color c) noexcept
{
    const double target = c == Color::A ? 1.0 : 0.0;
    if (target == colorTarget)
        return;
    colorTarget = target;
    currentColor = c;
    colorStep = (target - colorMix) / std::max (1.0, 0.02 * fs);
    setTapTargets (c, false);
}

void Line::setExtraDelay (double seconds, bool immediate) noexcept
{
    if (seconds == extraDelay && ! immediate)
        return;
    extraDelay = seconds;
    setTapTargets (currentColor, immediate);
}

double Line::readThiran (Tap& t) noexcept
{
    if (t.step != 0.0)
    {
        t.delay += t.step;
        if ((t.step > 0.0 && t.delay >= t.target) || (t.step < 0.0 && t.delay <= t.target))
        {
            t.delay = t.target;
            t.step = 0.0;
        }
    }
    const double m = std::floor (t.delay - 0.5);
    const double d = t.delay - m;
    const double a = (1.0 - d) / (1.0 + d);
    const size_t n = buffer.size();
    const size_t mi = (size_t) m;
    const double x0 = buffer[(writePos + n - mi) % n];
    const double x1 = buffer[(writePos + n - mi - 1) % n];
    t.thiranY = a * x0 + x1 - a * t.thiranY;
    return t.thiranY;
}

double Line::gaussian() noexcept
{
    if (haveSpare)
    {
        haveSpare = false;
        return spare;
    }
    auto next = [this]
    {
        rngState ^= rngState >> 12;
        rngState ^= rngState << 25;
        rngState ^= rngState >> 27;
        return ((rngState * 0x2545F4914F6CDD1Dull) >> 11) * (1.0 / 9007199254740992.0);
    };
    double u1 = next();
    while (u1 <= 0.0)
        u1 = next();
    const double u2 = next();
    const double r = std::sqrt (-2.0 * std::log (u1));
    spare = r * std::sin (kTwoPi * u2);
    haveSpare = true;
    return r * std::cos (kTwoPi * u2);
}

double Line::process (double x, double drive, const LineSwitches& sw) noexcept
{
    double y = x * drive;
    if (sw.nonlinear)
        y = inXfmr.process (y);

    y = rc.process (send.process (y));

    if (sw.nonlinear)
    {
        const double e = excursion.process (y);
        const double hit = e >= 0.0 ? knee (e, limitPos) : -knee (-e, limitNeg);
        y += hit - e;
    }

    y = transducer.process (y);
    if (colorStep != 0.0)
    {
        colorMix += colorStep;
        if ((colorStep > 0.0 && colorMix >= colorTarget) || (colorStep < 0.0 && colorMix <= colorTarget))
        {
            colorMix = colorTarget;
            colorStep = 0.0;
        }
    }
    const double withResiduals = residuals.process (y);
    y += colorMix * (withResiduals - y);
    y = stubHp.process (y);

    if (sw.nonlinear)
    {
        const double sq = y * y;
        y += kAir * fs * (sq - prevSq);
        prevSq = sq;
    }

    buffer[writePos] = wallLoss.process (y);
    double out = readThiran (direct);
    const double echoIn = readThiran (echoTap);
    if (echoOn)
        out += kEchoReflection * echoWallLoss.process (echoIn);
    if (++writePos == buffer.size())
        writePos = 0;
    out *= makeup;

    if (sw.noise != NoiseMode::Off)
        out += noiseSigma * (sw.noise == NoiseMode::Extra ? 3.9810717055349722 : 1.0) * gaussian();

    out = receive.process (out);
    if (sw.nonlinear)
        out = outXfmr.process (out);
    return out / drive;
}

} // namespace rrd
