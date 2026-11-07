// Spec tests for the real-time engine (doc 03 section 7). Run: ./rrd_tests
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "Engine.h"

using namespace rrd;

namespace
{
int failures = 0;
int checks = 0;
bool verbose = false;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
        ++failures;
    if (! ok || verbose)
        std::printf ("  %s: %s\n", ok ? "ok" : "FAIL", what.c_str());
}

std::string fmt (const char* f, double a, double b = 0.0, double c = 0.0)
{
    char buf[256];
    std::snprintf (buf, sizeof buf, f, a, b, c);
    return buf;
}

constexpr double kPre = 0.05;

EngineParams linearParams (Color c = Color::B, Routing r = Routing::Widener)
{
    EngineParams p;
    p.color = c;
    p.routing = r;
    p.nonlinear = false;
    p.noise = NoiseMode::Off;
    p.crosstalk = false;
    return p;
}

struct Run
{
    std::vector<double> left, right;
    int latency = 0;
};

Run run (double fs, const EngineSetup& setup, const EngineParams& params, const std::function<double (int)>& inL,
         const std::function<double (int)>& inR, double seconds)
{
    Engine e;
    e.prepare (fs, setup, params);
    Run r;
    r.latency = e.latencySamples();
    const int n = (int) (seconds * fs);
    r.left.resize ((size_t) n);
    r.right.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        e.processSample (inL (i), inR (i), r.left[(size_t) i], r.right[(size_t) i]);
    return r;
}

/** Impulse at kPre seconds; returns the responses aligned so index 0 = the impulse (latency removed). */
Run impulse (double fs, const EngineSetup& setup, const EngineParams& params, double seconds, bool left = true, bool right = true)
{
    const int at = (int) (kPre * fs);
    auto imp = [at] (int i) { return i == at ? 1.0 : 0.0; };
    auto zero = [] (int) { return 0.0; };
    Run r = run (fs, setup, params, left ? std::function<double (int)> (imp) : zero,
                 right ? std::function<double (int)> (imp) : zero, seconds + kPre + 0.01);
    const size_t off = (size_t) (at + r.latency);
    r.left.erase (r.left.begin(), r.left.begin() + (std::ptrdiff_t) off);
    r.right.erase (r.right.begin(), r.right.begin() + (std::ptrdiff_t) off);
    return r;
}

cplx dtft (const std::vector<double>& y, double f, double fs)
{
    cplx acc = 0.0;
    const cplx step = std::polar (1.0, -kTwoPi * f / fs);
    cplx w = 1.0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        acc += y[i] * w;
        w *= step;
        if ((i & 1023) == 1023)
            w /= std::abs (w);
    }
    return acc;
}

double dB (double x) { return 20.0 * std::log10 (std::max (x, 1e-30)); }

/** Group delay (s) at 1 kHz from a phase fit over 800-1200 Hz in 2 Hz steps. */
double groupDelay1k (const std::vector<double>& y, double fs)
{
    std::vector<double> f, ph;
    double prev = 0.0, offset = 0.0;
    for (double q = 800.0; q <= 1200.0; q += 2.0)
    {
        double a = std::arg (dtft (y, q, fs));
        if (! ph.empty())
        {
            while (a + offset - prev > kPi)
                offset -= kTwoPi;
            while (a + offset - prev < -kPi)
                offset += kTwoPi;
        }
        prev = a + offset;
        f.push_back (q);
        ph.push_back (prev);
    }
    double mf = 0, mp = 0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        mf += f[i];
        mp += ph[i];
    }
    mf /= (double) f.size();
    mp /= (double) f.size();
    double num = 0, den = 0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        num += (f[i] - mf) * (ph[i] - mp);
        den += (f[i] - mf) * (f[i] - mf);
    }
    return -(num / den) / kTwoPi;
}

std::vector<double> logFreqs (double lo, double hi, int n)
{
    std::vector<double> f;
    for (int i = 0; i < n; ++i)
        f.push_back (lo * std::pow (hi / lo, i / (n - 1.0)));
    return f;
}

double tonePeak (double dbu) { return std::pow (10.0, (dbu - 4.0 + Line::kNominalDbfs) / 20.0) * std::sqrt (2.0); }

/** THD (%) of the left output for a sine through line A. */
double thd (double fs, double freq, double dbu, const EngineSetup& setup)
{
    EngineParams p;
    p.noise = NoiseMode::Off;
    p.crosstalk = false;
    p.routing = Routing::Dual;
    const double a = tonePeak (dbu);
    Run r = run (fs, setup, p, [&] (int i) { return a * std::sin (kTwoPi * freq * i / fs); }, [] (int) { return 0.0; }, 1.0);
    std::vector<double> seg (r.left.begin() + (std::ptrdiff_t) (0.4 * fs), r.left.begin() + (std::ptrdiff_t) (0.9 * fs));
    const double h1 = std::abs (dtft (seg, freq, fs));
    double hs = 0.0;
    for (int h = 2; h <= 7 && h * freq < fs / 2; ++h)
        hs += std::norm (dtft (seg, h * freq, fs));
    return 100.0 * std::sqrt (hs) / h1;
}

void testDelayAccuracy()
{
    std::puts ("delay accuracy (group delay at 1 kHz, +-0.05 samples)");
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int os : { 1, 2, 4 })
        {
            if (fs * os > 400000.0)
                continue;
            EngineSetup s;
            s.echo = false;
            s.oversampling = os;
            Run r = impulse (fs, s, linearParams (Color::B, Routing::Dual), 0.6);
            for (auto [y, tau] : { std::pair { &r.left, Engine::kTauA }, std::pair { &r.right, Engine::kTauB } })
            {
                const double err = (groupDelay1k (*y, fs) - tau) * fs;
                check (std::abs (err) < 0.05, fmt ("fs %.0f os %.0f: delay error %.4f samples", fs, os, err));
            }
        }
    for (int os : { 1, 2, 4 })
    {
        EngineSetup s;
        s.echo = false;
        s.oversampling = os;
        Run r = impulse (48000.0, s, linearParams (Color::B, Routing::Cascade), 0.6);
        const double err = (groupDelay1k (r.left, 48000.0) - Engine::kTauA - Engine::kTauB) * 48000.0;
        check (std::abs (err) < 0.05, fmt ("cascade os %.0f: delay error %.4f samples", os, err));
    }
}

void testFrequencyResponse()
{
    std::puts ("frequency response (Color B within +-0.7 dB 40 Hz-10 kHz, steep above 11 kHz; Color A within +-2 dB)");
    for (int os : { 1, 2, 4 })
        for (Color c : { Color::B, Color::A })
        {
            EngineSetup s;
            s.oversampling = os;
            Run r = impulse (48000.0, s, linearParams (c, Routing::Dual), 1.0);
            for (auto* y : { &r.left, &r.right })
            {
                const double ref = std::abs (dtft (*y, 1000.0, 48000.0));
                double lo = 1e9, hi = -1e9;
                for (double f : logFreqs (40.0, 10000.0, 300))
                {
                    const double m = dB (std::abs (dtft (*y, f, 48000.0)) / ref);
                    lo = std::min (lo, m);
                    hi = std::max (hi, m);
                }
                const double tol = c == Color::B ? 0.7 : 2.0;
                check (lo >= -tol && hi <= tol, std::string (c == Color::A ? "Color A" : "Color B")
                                                     + fmt (" os %.0f: span %.2f..%.2f dB", os, lo, hi));
                check (std::abs (dB (ref)) < 0.2, fmt ("unity gain at 1 kHz: %.3f dB", dB (ref)));
                const double at12 = dB (std::abs (dtft (*y, 12000.0, 48000.0)) / ref);
                const double at15 = dB (std::abs (dtft (*y, 15000.0, 48000.0)) / ref);
                check (at12 < -6.0 && at15 < -20.0, fmt ("rolloff: %.1f dB at 12 kHz, %.1f dB at 15 kHz", at12, at15));
                if (c == Color::A)
                {
                    const double at7 = dB (std::abs (dtft (*y, 7000.0, 48000.0)) / ref);
                    check (at7 < -0.7, fmt ("Color A 7 kHz notch: %.2f dB", at7));
                }
            }
        }
}

void testEcho()
{
    std::puts ("echo (peak at 3 tau +-0.5 ms, level vs model +-2 dB, cascade beat null ~120 Hz)");
    EngineSetup on, off;
    off.echo = false;
    Run a = impulse (48000.0, on, linearParams (Color::B, Routing::Dual), 1.0);
    Run b = impulse (48000.0, off, linearParams (Color::B, Routing::Dual), 1.0);
    const double model[2][2] = { { -28.0, -43.4 }, { -27.1, -40.5 } }; // from the reference model at 100 Hz / 1 kHz
    int idx = 0;
    for (auto [ya, yb, tau] : { std::tuple { &a.left, &b.left, Engine::kTauA }, std::tuple { &a.right, &b.right, Engine::kTauB } })
    {
        std::vector<double> echo (ya->size());
        size_t peak = 0;
        for (size_t i = 0; i < echo.size(); ++i)
        {
            echo[i] = (*ya)[i] - (*yb)[i];
            if (std::abs (echo[i]) > std::abs (echo[peak]))
                peak = i;
        }
        const double t = peak / 48000.0;
        check (std::abs (t - 3.0 * tau) < 0.5e-3, fmt ("echo peak at %.2f ms (3 tau = %.1f ms)", t * 1e3, 3e3 * tau));
        int k = 0;
        for (double f : { 100.0, 1000.0 })
        {
            const double level = dB (std::abs (dtft (echo, f, 48000.0)) / std::abs (dtft (*yb, f, 48000.0)));
            check (std::abs (level - model[idx][k]) < 2.0, fmt ("echo level at %.0f Hz: %.1f dB (model %.1f)", f, level, model[idx][k]));
            ++k;
        }
        ++idx;
    }

    Run ca = impulse (48000.0, on, linearParams (Color::B, Routing::Cascade), 1.0);
    Run cb = impulse (48000.0, off, linearParams (Color::B, Routing::Cascade), 1.0);
    std::vector<double> e (ca.left.size());
    for (size_t i = 0; i < e.size(); ++i)
        e[i] = ca.left[i] - cb.left[i];
    double best = 0.0, bestMag = 1e9;
    for (double f = 80.0; f <= 200.0; f += 0.5)
    {
        const double m = std::abs (dtft (e, f, 48000.0));
        if (m < bestMag)
        {
            bestMag = m;
            best = f;
        }
    }
    check (std::abs (best - 120.0) < 3.0, fmt ("cascade beat null at %.1f Hz", best));
}

void testDistortion()
{
    std::puts ("distortion (THD at +4 dBu: <= 0.5% at 1 kHz, < 1% 100 Hz-8 kHz; driver strike rises with level)");
    EngineSetup s;
    const double t1k = thd (48000.0, 1000.0, 4.0, s);
    check (t1k <= 0.5, fmt ("1 kHz: %.3f%%", t1k));
    for (double f : { 100.0, 2000.0, 5000.0, 8000.0 })
    {
        const double t = thd (48000.0, f, 4.0, s);
        check (t < 1.0, fmt ("%.0f Hz: %.3f%%", f, t));
    }
    const double low = thd (48000.0, 100.0, 4.0, s), hot = thd (48000.0, 100.0, 16.0, s);
    check (low < 0.1 && hot > 3.0, fmt ("100 Hz: %.3f%% at +4 dBu, %.2f%% at +16 dBu", low, hot));
}

void testNoiseAndCrosstalk()
{
    std::puts ("noise (S/N 70-76 dB in 15.7 kHz) and crosstalk (-45 dB at 1 kHz, -25 dB at 10 kHz)");
    EngineSetup s;
    EngineParams p;
    p.routing = Routing::Dual;
    p.crosstalk = false;
    Run r = run (48000.0, s, p, [] (int) { return 0.0; }, [] (int) { return 0.0; }, 3.0);
    std::vector<double> y (r.left.begin() + 48000, r.left.end());
    // band-limit to 15.7 kHz with a brick-wall in the DFT domain (coarse 5 Hz grid is enough for RMS)
    double power = 0.0;
    const double df = 5.0;
    for (double f = df; f <= Line::kNoiseBandwidthHz; f += df)
        power += 2.0 * std::norm (dtft (y, f, 48000.0)) / ((double) y.size() * y.size()) * (df * y.size() / 48000.0);
    const double sn = Line::kNominalDbfs - 10.0 * std::log10 (power);
    check (sn >= 70.0 && sn <= 76.0, fmt ("S/N %.1f dB", sn));

    EngineParams x = linearParams (Color::B, Routing::Dual);
    x.crosstalk = true;
    for (auto [f, spec] : { std::pair { 1000.0, -45.0 }, std::pair { 10000.0, -25.0 } })
    {
        const double a = tonePeak (4.0);
        Run c = run (48000.0, s, x, [] (int) { return 0.0; }, [&] (int i) { return a * std::sin (kTwoPi * f * i / 48000.0); }, 1.0);
        std::vector<double> seg (c.left.begin() + 24000, c.left.end());
        const double level = dB (2.0 * std::abs (dtft (seg, f, 48000.0)) / seg.size() / a);
        check (std::abs (level - spec) < 1.5, fmt ("feed B read A at %.0f Hz: %.1f dB", f, level));
    }
}

void testDryAlignmentAndRoutings()
{
    std::puts ("dry path, latency compensation and routings");
    for (int os : { 1, 2, 4 })
    {
        EngineSetup s;
        s.oversampling = os;
        EngineParams p = linearParams();
        p.mix = 0.0;
        Run r = impulse (48000.0, s, p, 0.1);
        check (std::abs (r.left[0] - 1.0) < 1e-12 && std::abs (r.right[0] - 1.0) < 1e-12,
               fmt ("os %.0f: dry is the input delayed by exactly the reported latency (%.0f)", os, r.latency));
    }
    Run w = impulse (48000.0, EngineSetup {}, linearParams (Color::B, Routing::Widener), 0.1);
    auto peakIndex = [] (const std::vector<double>& y)
    {
        size_t k = 0;
        for (size_t i = 0; i < y.size(); ++i)
            if (std::abs (y[i]) > std::abs (y[k]))
                k = i;
        return (double) k;
    };
    check (std::abs (peakIndex (w.left) - 768.0) <= 1.0 && std::abs (peakIndex (w.right) - 672.0) <= 1.0,
           fmt ("widener: left peak %.0f (768), right peak %.0f (672)", peakIndex (w.left), peakIndex (w.right)));
}

void testBypass()
{
    std::puts ("bypass keeps the reported latency and switches seamlessly");
    for (int os : { 1, 2, 4 })
    {
        EngineSetup s;
        s.oversampling = os;
        EngineParams p = linearParams();
        p.mix = 0.0;
        Engine e;
        e.prepare (48000.0, s, p);
        const int lat = e.latencySamples();
        std::vector<float> l (4096), r (4096), inL (4096), inR (4096);
        for (size_t i = 0; i < l.size(); ++i)
        {
            inL[i] = l[i] = (float) std::sin (0.01 * (double) i);
            inR[i] = r[i] = (float) std::cos (0.013 * (double) i);
        }
        // Alternate processed (dry only) and bypassed blocks: the output must be one continuous delayed copy.
        for (int start = 0, k = 0; start < 4096; start += 256, ++k)
            std::invoke (k % 2 ? &Engine::processBypassed : &Engine::process, e, l.data() + start, r.data() + start, 256);
        double err = 0.0;
        for (size_t i = (size_t) lat; i < l.size(); ++i)
            err = std::max ({ err, std::abs ((double) l[i] - inL[i - (size_t) lat]), std::abs ((double) r[i] - inR[i - (size_t) lat]) });
        check (err < 1e-6, fmt ("os %.0f: bypass output = input delayed by %.0f samples (max error %.1e)", os, lat, err));
    }
}

void testDoubler()
{
    std::puts ("doubler (dry and 30 ms cascade as two panned sources, constant-power pan, mono fold-down)");
    auto params = [] (double dryPan, double wetPan, double dryDb, double wetDb)
    {
        EngineParams p = linearParams (Color::B, Routing::Cascade);
        p.doubler = true;
        p.dryPan = dryPan;
        p.wetPan = wetPan;
        p.dryLevelDb = dryDb;
        p.wetLevelDb = wetDb;
        return p;
    };
    auto peak = [] (const std::vector<double>& y, size_t from, size_t to)
    {
        double m = 0.0;
        for (size_t i = from; i < std::min (to, y.size()); ++i)
            m = std::max (m, std::abs (y[i]));
        return m;
    };
    const size_t tap = (size_t) std::lround ((Engine::kTauA + Engine::kTauB) * 48000.0);

    // Hard pans: the dry impulse only on the left at t = 0, the cascade only on the right around 30 ms.
    Run h = impulse (48000.0, EngineSetup {}, params (-1.0, 1.0, 0.0, 0.0), 0.1);
    check (std::abs (h.left[0] - 1.0) < 1e-9 && std::abs (h.right[0]) < 1e-9, fmt ("hard left dry: L %.3f, R %.3f", h.left[0], h.right[0]));
    check (peak (h.left, 1, h.left.size()) < 1e-9, fmt ("nothing else on the left (peak %.1e)", peak (h.left, 1, h.left.size())));
    check (peak (h.right, tap - 40, tap + 40) > 0.2 && peak (h.right, 0, tap - 200) < 1e-6,
           fmt ("cascade on the right at 30 ms (peak %.3f, before %.1e)", peak (h.right, tap - 40, tap + 40), peak (h.right, 0, tap - 200)));

    // Centre pan is -3 dB per side; level knobs scale each source independently.
    Run c = impulse (48000.0, EngineSetup {}, params (0.0, 1.0, -6.0, 0.0), 0.05);
    const double expect = std::pow (10.0, -6.0 / 20.0) * std::sqrt (0.5);
    check (std::abs (c.left[0] - expect) < 1e-9 && std::abs (c.right[0] - expect) < 1e-9,
           fmt ("centred dry at -6 dB: L %.4f, R %.4f (expected %.4f)", c.left[0], c.right[0], expect));

    // Mono output ignores the pans and keeps both sources.
    EngineParams p = params (-1.0, 1.0, 0.0, 0.0);
    p.noise = NoiseMode::Off;
    Engine e;
    e.prepare (48000.0, EngineSetup {}, p);
    std::vector<float> mono (4800, 0.0f);
    mono[100] = 1.0f;
    e.process (mono.data(), nullptr, (int) mono.size());
    std::vector<double> y (mono.begin() + 100 + e.latencySamples(), mono.end());
    check (std::abs (y[0] - 1.0) < 1e-6 && peak (y, tap - 40, tap + 40) > 0.2,
           fmt ("mono output: dry %.3f, cascade peak %.3f", y[0], peak (y, tap - 40, tap + 40)));
}

/** A guitar-like note: a few harmonics, 1 ms attack, exponential decay. */
double pluck (double t, double f, double tau)
{
    if (t < 0.0)
        return 0.0;
    const double env = std::min (1.0, t / 0.001) * std::exp (-t / tau);
    return 0.25 * env * (std::sin (kTwoPi * f * t) + 0.5 * std::sin (kTwoPi * 2.0 * f * t) + 0.25 * std::sin (kTwoPi * 3.0 * f * t));
}

/** Lag (samples) that best aligns out[s + lag ..] with in[s ..] over n samples, searched around `nominal`. */
double bestLag (const std::vector<double>& in, const std::vector<double>& out, size_t s, size_t n, double nominal, double range)
{
    auto corr = [&] (long lag)
    {
        double acc = 0.0, e1 = 0.0, e2 = 0.0;
        for (size_t i = s; i < s + n; ++i)
        {
            const double a = in[i], b = out[(size_t) ((long) i + lag)];
            acc += a * b;
            e1 += a * a;
            e2 += b * b;
        }
        return acc / std::sqrt (e1 * e2 + 1e-30);
    };
    long best = (long) (nominal - range);
    double bestC = -2.0;
    for (long lag = (long) (nominal - range); lag <= (long) (nominal + range); ++lag)
        if (const double c = corr (lag); c > bestC)
        {
            bestC = c;
            best = lag;
        }
    const double a = corr (best - 1), b = bestC, c = corr (best + 1);
    return (double) best + 0.5 * (a - c) / (a - 2.0 * b + c);
}

void testHumanize()
{
    std::puts ("humanize (one random shift per note within +-amount, held for the whole note; off is exact)");
    // Decaying noise bursts: broadband, so the alignment is unambiguous (a pitched tone would match every period).
    // Spaced 1 s apart so successive shifts are mostly independent (closer notes are deliberately correlated).
    const double fs = 48000.0, period = 1.0, amount = 5.0;
    const int notes = 24;
    const int n = (int) ((notes + 1) * period * fs);
    std::vector<double> in ((size_t) n);
    std::mt19937 rng (11);
    std::normal_distribution<double> g (0.0, 0.15);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, local = t - (int) (t / period) * period;
        in[(size_t) i] = g (rng) * std::min (1.0, local / 0.001) * std::exp (-local / 0.08);
    }
    auto render = [&] (bool human)
    {
        EngineParams p = linearParams (Color::B, Routing::Cascade);
        p.doubler = true;
        p.dryPan = -1.0;
        p.wetPan = 1.0;
        p.human = human;
        p.humanMs = amount;
        return run (fs, EngineSetup {}, p, [&] (int i) { return in[(size_t) i]; }, [&] (int i) { return in[(size_t) i]; }, n / fs).right;
    };
    const auto robot = render (false), human = render (true);
    const double nominal = 0.030 * fs + 31.0, range = 0.012 * fs;

    std::vector<double> shifts;
    double worstWithin = 0.0, worstRobot = 0.0;
    for (int k = 1; k < notes; ++k)
    {
        const size_t start = (size_t) (k * period * fs);
        const size_t early = start, late = start + (size_t) (0.06 * fs);
        const size_t lenEarly = (size_t) (0.03 * fs), lenLate = (size_t) (0.06 * fs);
        const double r1 = bestLag (in, robot, early, lenEarly, nominal, range), r2 = bestLag (in, robot, late, lenLate, nominal, range);
        const double h1 = bestLag (in, human, early, lenEarly, nominal, range), h2 = bestLag (in, human, late, lenLate, nominal, range);
        worstRobot = std::max (worstRobot, std::abs (r1 - r2) / fs * 1e3);
        worstWithin = std::max (worstWithin, std::abs ((h1 - r1) - (h2 - r2)) / fs * 1e3);
        shifts.push_back ((h1 - r1) / fs * 1e3);
    }
    const auto [lo, hi] = std::minmax_element (shifts.begin(), shifts.end());
    double mean = 0.0, var = 0.0;
    for (double x : shifts)
        mean += x / (double) shifts.size();
    for (double x : shifts)
        var += (x - mean) * (x - mean) / (double) shifts.size();
    check (worstWithin < 0.05, fmt ("shift is constant within each note (worst early/late difference %.3f ms)", worstWithin));
    check (*lo >= -amount - 0.05 && *hi <= amount + 0.05, fmt ("shifts within +-%.0f ms: %.2f..%.2f ms", amount, *lo, *hi));
    check (*lo < -0.25 * amount && *hi > 0.25 * amount && std::sqrt (var) > 0.5,
           fmt ("shifts go both ways and vary note to note (%.2f..%.2f ms, spread %.2f ms)", *lo, *hi, std::sqrt (var)));
    check (worstRobot < 0.05, fmt ("humanize off: timing identical early and late in each note (worst %.3f ms)", worstRobot));
}

void testHumanizerDetector()
{
    std::puts ("humanize note detection (picked notes, legato hammer-ons, slides)");
    const double fs = 48000.0;
    auto runHumanizer = [&] (const std::function<double (double)>& signal, double seconds, const std::vector<double>& probeTimes)
    {
        Humanizer h;
        h.prepare (fs, 3);
        std::vector<double> probes;
        size_t next = 0;
        for (int i = 0; i < (int) (seconds * fs); ++i)
        {
            h.process (signal (i / fs), true, 5.0);
            if (next < probeTimes.size() && i / fs >= probeTimes[next])
            {
                probes.push_back (h.currentShiftMs());
                ++next;
            }
        }
        return std::pair { h.onsets(), probes };
    };
    auto meanStep = [] (const std::vector<double>& v)
    {
        double s = 0.0;
        for (size_t i = 1; i < v.size(); ++i)
            s += std::abs (v[i] - v[i - 1]) / (double) (v.size() - 1);
        return s;
    };

    // Picked notes with gaps: one onset per note, and fresh shifts.
    std::vector<double> picks;
    for (int k = 0; k < 16; ++k)
        picks.push_back (0.1 + k * 1.2 + 0.02);
    const auto [pickOnsets, pickShifts] = runHumanizer ([] (double t)
    {
        const int k = (int) ((t - 0.1) / 1.2);
        return t < 0.1 ? 0.0 : pluck (t - 0.1 - k * 1.2, 220.0 * std::pow (2.0, (k % 5) / 12.0), 0.25);
    }, 0.1 + 16 * 1.2, picks);
    check (pickOnsets == 16, fmt ("picked notes: %.0f onsets for 16 notes", pickOnsets));

    // Legato: a ringing string, a new pitch every 150 ms with a hammer-on bump of +6 dB.
    std::vector<double> legatoProbes;
    for (int k = 0; k < 40; ++k)
        legatoProbes.push_back (0.1 + k * 0.15 + 0.02);
    double phase = 0.0;
    double lastT = 0.0;
    const auto [legatoOnsets, legatoShifts] = runHumanizer ([&] (double t)
    {
        if (t < 0.1)
            return 0.0;
        const int k = (int) ((t - 0.1) / 0.15);
        const double local = t - 0.1 - k * 0.15;
        const double f = 196.0 * std::pow (2.0, (k % 5) / 12.0);
        phase += kTwoPi * f * (t - lastT);
        lastT = t;
        const double env = 0.12 * (1.0 + std::exp (-local / 0.03)) * std::min (1.0, (t - 0.1) / 0.001);
        return env * (std::sin (phase) + 0.5 * std::sin (2.0 * phase) + 0.3 * std::sin (3.0 * phase));
    }, 0.1 + 40 * 0.15, legatoProbes);
    check (std::abs (legatoOnsets - 40) <= 1, fmt ("legato hammer-ons: %.0f onsets for 40 notes", legatoOnsets));
    check (meanStep (legatoShifts) < 0.6 * meanStep (pickShifts),
           fmt ("legato runs move by smaller steps (%.2f ms) than separate phrases (%.2f ms)", meanStep (legatoShifts), meanStep (pickShifts)));

    // A slide: continuous pitch glide at constant level, no new attack.
    phase = 0.0;
    lastT = 0.0;
    const auto [slideOnsets, unused] = runHumanizer ([&] (double t)
    {
        if (t < 0.1)
            return 0.0;
        const double f = 196.0 * std::pow (2.0, std::min (1.0, (t - 0.3) / 0.6) * std::max (0.0, 1.0) * (t > 0.3 ? 5.0 / 12.0 : 0.0));
        phase += kTwoPi * f * (t - lastT);
        lastT = t;
        return 0.2 * std::min (1.0, (t - 0.1) / 0.001) * (std::sin (phase) + 0.5 * std::sin (2.0 * phase));
    }, 1.5, {});
    check (slideOnsets == 1, fmt ("slide: %.0f onset (the first attack only)", slideOnsets));
}

void testStress()
{
    std::puts ("automation stress (no NaN/Inf, bounded output, smooth color/routing switches)");
    Engine e;
    EngineSetup s;
    EngineParams p;
    e.prepare (48000.0, s, p);
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (-1.0, 1.0);
    bool finite = true;
    double peak = 0.0;
    for (int block = 0; block < 2000; ++block)
    {
        p.driveDb = 20.0 * std::sin (block * 0.05);
        p.mix = 0.5 + 0.5 * std::sin (block * 0.031);
        p.color = (block / 37) % 2 ? Color::A : Color::B;
        p.routing = static_cast<Routing> ((block / 101) % 3);
        p.noise = static_cast<NoiseMode> ((block / 53) % 3);
        p.levelADb = -10.0 + 10.0 * std::sin (block * 0.02);
        p.doubler = (block / 71) % 2 == 1;
        p.dryPan = std::sin (block * 0.013);
        p.wetPan = -p.dryPan;
        p.human = (block / 43) % 2 == 1;
        p.humanMs = 10.0 * std::abs (std::sin (block * 0.007));
        e.setParams (p);
        for (int i = 0; i < 64; ++i)
        {
            double l, r;
            const double x = (block % 200 < 100) ? u (rng) : 0.0;
            e.processSample (x, -x, l, r);
            finite = finite && std::isfinite (l) && std::isfinite (r);
            peak = std::max ({ peak, std::abs (l), std::abs (r) });
        }
    }
    check (finite, "all outputs finite");
    check (peak < 20.0, fmt ("peak output %.2f", peak));
}

} // namespace

int main (int argc, char** argv)
{
    verbose = argc > 1 && std::string (argv[1]) == "-v";
    testDryAlignmentAndRoutings();
    testFrequencyResponse();
    testDelayAccuracy();
    testEcho();
    testDistortion();
    testNoiseAndCrosstalk();
    testBypass();
    testDoubler();
    testHumanize();
    testHumanizerDetector();
    testStress();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
