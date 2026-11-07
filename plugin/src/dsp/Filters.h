#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <span>
#include <vector>

namespace rrd
{
using cplx = std::complex<double>;
constexpr double kPi = std::numbers::pi;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

/** Transposed direct form II biquad, double precision. a0 is normalized to 1. */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1 = 0.0, s2 = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { s1 = s2 = 0.0; }

    cplx response (double freqHz, double fs) const noexcept
    {
        const cplx zi = std::polar (1.0, -kTwoPi * freqHz / fs);
        return (b0 + zi * (b1 + zi * b2)) / (1.0 + zi * (a1 + zi * a2));
    }
};

/** A cascade of biquads (first-order sections are stored as biquads with b2 = a2 = 0). */
class Cascade
{
public:
    void setSections (std::vector<Biquad> s)
    {
        sections = std::move (s);
        reset();
    }

    double process (double x) noexcept
    {
        for (auto& s : sections)
            x = s.process (x);
        return x;
    }

    void reset() noexcept
    {
        for (auto& s : sections)
            s.reset();
    }

    cplx response (double freqHz, double fs) const noexcept
    {
        cplx h { 1.0, 0.0 };
        for (const auto& s : sections)
            h *= s.response (freqHz, fs);
        return h;
    }

    void scale (double g) noexcept
    {
        if (sections.empty())
            sections.push_back ({});
        auto& s = sections.front();
        s.b0 *= g;
        s.b1 *= g;
        s.b2 *= g;
    }

    /** Scales the cascade so its magnitude at refHz is exactly 1. */
    void normalizeAt (double refHz, double fs) noexcept { scale (1.0 / std::abs (response (refHz, fs))); }

    size_t size() const noexcept { return sections.size(); }

private:
    std::vector<Biquad> sections;
};

/** Analog zeros/poles in rad/s. Gain is set later by normalizing the digital cascade. */
struct AnalogZpk
{
    std::vector<cplx> zeros, poles;

    AnalogZpk& append (const AnalogZpk& o)
    {
        zeros.insert (zeros.end(), o.zeros.begin(), o.zeros.end());
        poles.insert (poles.end(), o.poles.begin(), o.poles.end());
        return *this;
    }

    AnalogZpk inverse() const { return { poles, zeros }; }
};

/** Roots of a real polynomial (coefficients highest power first) by Aberth-Ehrlich iteration. */
inline std::vector<cplx> polyRoots (std::span<const double> coeffs)
{
    size_t lead = 0;
    while (lead < coeffs.size() && ! (std::abs (coeffs[lead]) > 0.0))
        ++lead;
    const auto c = coeffs.subspan (lead);
    const int n = static_cast<int> (c.size()) - 1;
    if (n < 1)
        return {};

    std::vector<cplx> a (c.begin(), c.end());
    for (auto& v : a)
        v /= c[0];

    auto evalWithDerivative = [&a, n] (cplx x, cplx& d)
    {
        cplx p = a[0];
        d = 0.0;
        for (int i = 1; i <= n; ++i)
        {
            d = d * x + p;
            p = p * x + a[(size_t) i];
        }
        return p;
    };

    double radius = 0.0;
    for (int i = 1; i <= n; ++i)
        radius = std::max (radius, std::pow (std::abs (a[(size_t) i]), 1.0 / i));
    radius = std::max (radius, 1e-12);

    std::vector<cplx> z ((size_t) n);
    for (int k = 0; k < n; ++k)
        z[(size_t) k] = std::polar (radius, kTwoPi * (k + 0.25) / n);

    for (int iter = 0; iter < 500; ++iter)
    {
        double maxStep = 0.0;
        for (int k = 0; k < n; ++k)
        {
            cplx d;
            const cplx p = evalWithDerivative (z[(size_t) k], d);
            if (! (std::abs (p) > 0.0))
                continue;
            const cplx ratio = p / d;
            cplx repulsion = 0.0;
            for (int j = 0; j < n; ++j)
                if (j != k)
                    repulsion += 1.0 / (z[(size_t) k] - z[(size_t) j]);
            const cplx step = ratio / (1.0 - ratio * repulsion);
            z[(size_t) k] -= step;
            maxStep = std::max (maxStep, std::abs (step) / std::max (1.0, std::abs (z[(size_t) k])));
        }
        if (maxStep < 1e-15)
            break;
    }

    for (auto& r : z)
        if (std::abs (r.imag()) < 1e-10 * std::max (1.0, std::abs (r)))
            r = { r.real(), 0.0 };
    return z;
}

/** Bilinear transform with pre-warping at `warpHz`, grouped into biquads (conjugate pairs together,
    poles nearest the unit circle paired with their nearest zeros). Each section is normalized to unit
    magnitude at `refHz` so cascades stay well scaled. */
inline std::vector<Biquad> bilinearSections (const AnalogZpk& analog, double fs, double warpHz, double refHz = 1000.0)
{
    const double w0 = kTwoPi * warpHz;
    const double k = w0 / std::tan (w0 / (2.0 * fs));
    auto map = [k] (cplx s) { return (k + s) / (k - s); };

    std::vector<cplx> zs, ps;
    for (auto s : analog.zeros)
        zs.push_back (map (s));
    for (auto s : analog.poles)
        ps.push_back (map (s));
    while (zs.size() < ps.size())
        zs.push_back ({ -1.0, 0.0 });
    while (ps.size() < zs.size())
        ps.push_back ({ 0.0, 0.0 });

    auto isReal = [] (cplx v) { return ! (std::abs (v.imag()) > 0.0); }; // roots are snapped to the real axis
    auto takeGroup = [&isReal] (std::vector<cplx>& pool, size_t idx)
    {
        std::vector<cplx> g { pool[idx] };
        pool.erase (pool.begin() + (std::ptrdiff_t) idx);
        if (! isReal (g[0]))
        {
            auto it = std::min_element (pool.begin(), pool.end(), [&] (cplx a, cplx b)
                                        { return std::abs (a - std::conj (g[0])) < std::abs (b - std::conj (g[0])); });
            g.push_back (*it);
            pool.erase (it);
        }
        return g;
    };

    std::vector<Biquad> out;
    while (! ps.empty())
    {
        auto pIdx = (size_t) std::distance (ps.begin(), std::max_element (ps.begin(), ps.end(),
                                                [] (cplx a, cplx b) { return std::abs (a) < std::abs (b); }));
        auto poleGroup = takeGroup (ps, pIdx);
        if (poleGroup.size() == 1 && ! ps.empty())
        {
            auto it = std::find_if (ps.begin(), ps.end(), isReal);
            if (it != ps.end())
            {
                poleGroup.push_back (*it);
                ps.erase (it);
            }
        }

        std::vector<cplx> zeroGroup;
        const cplx target = poleGroup[0];
        auto byDistance = [&target] (cplx a, cplx b) { return std::abs (a - target) < std::abs (b - target); };
        auto zIt = std::min_element (zs.begin(), zs.end(), byDistance);
        if (poleGroup.size() == 1 || ! isReal (*zIt))
            zeroGroup = takeGroup (zs, (size_t) std::distance (zs.begin(), zIt));
        else
        {
            // two real zeros for a two-pole section: the nearest real one plus the next nearest real one
            zeroGroup.push_back (*zIt);
            zs.erase (zIt);
            auto next = std::min_element (zs.begin(), zs.end(), [&] (cplx a, cplx b)
                                          { return (isReal (a) ? std::abs (a - target) : 1e300) < (isReal (b) ? std::abs (b - target) : 1e300); });
            zeroGroup.push_back (*next);
            zs.erase (next);
        }

        Biquad b;
        auto coeffs = [] (const std::vector<cplx>& r, double& c1, double& c2)
        {
            c1 = r.size() > 0 ? -(r[0] + (r.size() > 1 ? r[1] : 0.0)).real() : 0.0;
            c2 = r.size() > 1 ? (r[0] * r[1]).real() : 0.0;
        };
        double n1, n2;
        coeffs (zeroGroup, n1, n2);
        coeffs (poleGroup, b.a1, b.a2);
        b.b0 = 1.0;
        b.b1 = n1;
        b.b2 = n2;
        const double g = 1.0 / std::abs (b.response (refHz, fs));
        b.b0 *= g;
        b.b1 *= g;
        b.b2 *= g;
        out.push_back (b);
    }
    return out;
}

/** Analog zeros/poles of a rational given in p = s / normRadPerSec. */
inline AnalogZpk zpkFromRational (std::span<const double> num, std::span<const double> den, double normRadPerSec)
{
    AnalogZpk z;
    for (auto r : polyRoots (num))
        z.zeros.push_back (r * normRadPerSec);
    for (auto r : polyRoots (den))
        z.poles.push_back (r * normRadPerSec);
    return z;
}

/** Analog Chebyshev type I low-pass poles (all-pole). */
inline AnalogZpk chebyshev1LowPass (int order, double rippleDb, double edgeHz)
{
    const double eps = std::sqrt (std::pow (10.0, rippleDb / 10.0) - 1.0);
    const double mu = std::asinh (1.0 / eps) / order;
    const double wc = kTwoPi * edgeHz;
    AnalogZpk z;
    for (int k = 1; k <= order; ++k)
    {
        const double theta = kPi * (2.0 * k - 1.0) / (2.0 * order);
        z.poles.push_back (wc * cplx (-std::sinh (mu) * std::sin (theta), std::cosh (mu) * std::cos (theta)));
    }
    for (auto& p : z.poles)
        if (std::abs (p.imag()) < 1e-9 * wc)
            p = { p.real(), 0.0 };
    return z;
}

/** Analog Butterworth poles; high-pass adds `order` zeros at DC. */
inline AnalogZpk butterworth (int order, double cornerHz, bool highPass)
{
    const double wc = kTwoPi * cornerHz;
    AnalogZpk z;
    for (int k = 1; k <= order; ++k)
    {
        const double theta = kPi * (2.0 * k + order - 1.0) / (2.0 * order);
        cplx p = std::polar (1.0, theta);
        if (std::abs (p.imag()) < 1e-12)
            p = { p.real(), 0.0 };
        z.poles.push_back (highPass ? wc / p : wc * p);
        if (highPass)
            z.zeros.push_back ({ 0.0, 0.0 });
    }
    for (auto& p : z.poles)
        if (std::abs (p.imag()) < 1e-9 * wc)
            p = { p.real(), 0.0 };
    return z;
}

inline std::vector<cplx> quadraticRoots (double b, double c)
{
    const cplx disc = std::sqrt (cplx (b * b - 4.0 * c, 0.0));
    cplx r1 = (-b + disc) / 2.0, r2 = (-b - disc) / 2.0;
    if (std::abs (r1.imag()) < 1e-12 * std::abs (r1))
        r1 = { r1.real(), 0.0 };
    if (std::abs (r2.imag()) < 1e-12 * std::abs (r2))
        r2 = { r2.real(), 0.0 };
    return { r1, r2 };
}

/** Analog constant-Q bell: (s^2 + s w0 A/Q + w0^2) / (s^2 + s w0 /(A Q) + w0^2). */
inline AnalogZpk bell (double f0, double gainDb, double q)
{
    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = kTwoPi * f0;
    return { quadraticRoots (w0 * a / q, w0 * w0), quadraticRoots (w0 / (a * q), w0 * w0) };
}

/** Group delay (s) of a response function at freqHz, by a +-1 Hz phase difference. */
template <typename ResponseFn>
double groupDelayAt (ResponseFn&& h, double freqHz)
{
    const cplx lo = h (freqHz - 1.0), hi = h (freqHz + 1.0);
    return -std::arg (hi / lo) / (kTwoPi * 2.0);
}

} // namespace rrd
