#pragma once

#include <cmath>
#include <vector>

#include "Filters.h"

namespace dfd
{
/** Linear-phase half-band FIR (Kaiser-windowed sinc). Odd length N = 4m + 3, so every other tap except the
    centre is zero and the group delay (N - 1) / 2 is an odd integer at the higher rate. */
class HalfBand
{
public:
    explicit HalfBand (int m = 15, double beta = 9.0)
    {
        const int n = 4 * m + 3;
        const int c = (n - 1) / 2;
        taps.assign ((size_t) n, 0.0);
        auto i0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 50; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };
        for (int i = 0; i < n; ++i)
        {
            const int k = i - c;
            const double sinc = k == 0 ? 0.5 : std::sin (kPi * k / 2.0) / (kPi * k);
            const double r = (double) k / c;
            taps[(size_t) i] = sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
        }
        double sum = 0.0;
        for (auto t : taps)
            sum += t;
        for (auto& t : taps)
            t /= sum;
        for (int i = 0; i < n; ++i)
            if ((i - c) % 2 != 0 || i == c)
                nonZero.push_back ((size_t) i);
        history.assign ((size_t) n, 0.0);
    }

    int length() const noexcept { return (int) taps.size(); }
    int delay() const noexcept { return (length() - 1) / 2; }

    void reset() { std::fill (history.begin(), history.end(), 0.0); }

    /** One input sample at the low rate -> two output samples at the high rate (gain 2 restores level). */
    void upsample (double x, double& y0, double& y1) noexcept
    {
        push (x);
        y0 = 2.0 * dot();
        push (0.0);
        y1 = 2.0 * dot();
    }

    /** Two input samples at the high rate -> one output sample at the low rate. */
    double downsample (double x0, double x1) noexcept
    {
        push (x0);
        const double y = dot();
        push (x1);
        return y;
    }

private:
    std::vector<double> taps, history;
    std::vector<size_t> nonZero; // half-band: every other tap except the centre is exactly zero
    size_t pos = 0;

    void push (double x) noexcept
    {
        pos = (pos == 0 ? history.size() : pos) - 1;
        history[pos] = x;
    }

    double dot() const noexcept
    {
        double acc = 0.0;
        const size_t n = taps.size();
        for (size_t i : nonZero)
        {
            size_t idx = pos + i;
            if (idx >= n)
                idx -= n;
            acc += taps[i] * history[idx];
        }
        return acc;
    }
};

/** 1x, 2x or 4x oversampling made of cascaded half-band stages. Linear phase; latency reported at the base rate. */
class Oversampler
{
public:
    void prepare (int factor)
    {
        stagesUp.clear();
        stagesDown.clear();
        factorValue = factor;
        if (factor >= 2)
        {
            stagesUp.emplace_back (15);
            stagesDown.emplace_back (15);
        }
        if (factor >= 4)
        {
            stagesUp.emplace_back (7);
            stagesDown.emplace_back (7);
        }
    }

    int factor() const noexcept { return factorValue; }

    /** Round-trip latency in base-rate samples (may be fractional for 4x). */
    double latency() const noexcept
    {
        double l = 0.0;
        double rate = 2.0;
        for (size_t i = 0; i < stagesUp.size(); ++i, rate *= 2.0)
            l += 2.0 * stagesUp[i].delay() / rate;
        return l;
    }

    void reset()
    {
        for (auto& s : stagesUp)
            s.reset();
        for (auto& s : stagesDown)
            s.reset();
    }

    /** Writes `factor()` high-rate samples for one base-rate input. */
    void up (double x, double* out) noexcept
    {
        if (factorValue == 1)
        {
            out[0] = x;
            return;
        }
        double a, b;
        stagesUp[0].upsample (x, a, b);
        if (factorValue == 2)
        {
            out[0] = a;
            out[1] = b;
            return;
        }
        stagesUp[1].upsample (a, out[0], out[1]);
        stagesUp[1].upsample (b, out[2], out[3]);
    }

    double down (const double* in) noexcept
    {
        if (factorValue == 1)
            return in[0];
        if (factorValue == 2)
            return stagesDown[0].downsample (in[0], in[1]);
        const double a = stagesDown[1].downsample (in[0], in[1]);
        const double b = stagesDown[1].downsample (in[2], in[3]);
        return stagesDown[0].downsample (a, b);
    }

private:
    std::vector<HalfBand> stagesUp, stagesDown;
    int factorValue = 1;
};

} // namespace dfd
