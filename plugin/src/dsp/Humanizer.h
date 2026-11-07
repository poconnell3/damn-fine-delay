#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rrd
{
/** Per-note timing offsets for the doubled copy, applied before the tubes.

    A fixed pre-delay (kBaseMs) with one read tap. A transient detector watches the input; at each new
    note the tap moves to base + a new random shift (within +-amount), via a short crossfade that is
    finished before the new note reaches either tap. Within a note the delay never moves, so there is
    no pitch bend and no sweep. Notes that follow closely get correlated shifts (a player stays ahead or
    behind through a phrase); after a gap the next shift is fresh. The engine subtracts kBaseMs from the
    tubes, so the net delay is 30 ms + shift. */
class Humanizer
{
public:
    static constexpr double kMaxShiftMs = 10.0;
    static constexpr double kFadeMs = 4.0;
    static constexpr double kBaseMs = kMaxShiftMs + 6.0;   // shortest tap leaves room for detection + fade
    static constexpr double kRefractoryMs = 50.0;          // fastest note rate that re-randomizes (20/s)
    static constexpr double kPhraseSeconds = 0.8;          // shift correlation time between notes
    static constexpr double kOnsetRatio = 1.6;             // fast/slow envelope, about +4 dB
    static constexpr double kFloor = 0.0018;               // about -55 dBFS after the detector high-pass

    void prepare (double sampleRate, uint64_t seed)
    {
        fs = sampleRate;
        base = (int) std::lround (kBaseMs * 1e-3 * fs);
        buffer.assign ((size_t) (base + (int) std::ceil (kMaxShiftMs * 1e-3 * fs) + 4), 0.0);
        hpCoeff = std::exp (-2.0 * 3.141592653589793 * 300.0 / fs);
        attack = 1.0 - std::exp (-1.0 / (0.0005 * fs));
        release = 1.0 - std::exp (-1.0 / (0.010 * fs));
        slowCoeff = 1.0 - std::exp (-1.0 / (0.025 * fs));
        refractory = (int) (kRefractoryMs * 1e-3 * fs);
        fadeStep = 1.0 / std::max (1.0, kFadeMs * 1e-3 * fs);
        rng = 0x9E3779B97F4A7C15ull ^ (seed * 0xBF58476D1CE4E5B9ull);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        write = 0;
        hp = xPrev = fast = slow = 0.0;
        sinceOnset = sinceNote = 1 << 30;
        unit = 0.0;
        tapOld = tapNew = base;
        fade = 1.0;
    }

    int baseDelaySamples() const noexcept { return base; }
    double currentShiftMs() const noexcept { return (tapNew - base) / fs * 1e3; }
    int onsets() const noexcept { return onsetCount; }

    /** One input sample in, the input delayed by base + the current note's shift out.
        enabled = false glides back to no shift; amountMs is read at each note onset. */
    double process (double x, bool enabled, double amountMs) noexcept
    {
        if (detect (x) && enabled)
        {
            // Correlated with the previous note's shift when the notes are close together; the mix keeps the
            // spread across the whole +-amount range whatever the note spacing.
            const double rho = std::exp (-(double) std::min (sinceNote, 1 << 24) / (kPhraseSeconds * fs));
            unit = std::clamp (rho * unit + std::sqrt (1.0 - rho * rho) * (2.0 * uniform() - 1.0), -1.0, 1.0);
            const double shift = std::clamp (amountMs, 0.0, kMaxShiftMs) * 1e-3 * fs * unit;
            moveTo (base + (int) std::lround (shift));
            sinceNote = 0;
        }
        else if (! enabled && tapNew != base && fade >= 1.0)
        {
            unit = 0.0;
            moveTo (base);
        }

        buffer[write] = x;
        double y = read (tapNew);
        if (fade < 1.0)
        {
            const double w = 0.5 - 0.5 * std::cos (3.141592653589793 * fade); // raised cosine, old + new = 1
            y = (1.0 - w) * read (tapOld) + w * y;
            fade = std::min (1.0, fade + fadeStep);
            if (fade >= 1.0)
                tapOld = tapNew;
        }
        if (++write == buffer.size())
            write = 0;
        ++sinceOnset;
        ++sinceNote;
        return y;
    }

private:
    double fs = 48000.0;
    int base = 768;
    std::vector<double> buffer;
    size_t write = 0;

    double hpCoeff = 0.0, attack = 0.0, release = 0.0, slowCoeff = 0.0;
    double hp = 0.0, xPrev = 0.0, fast = 0.0, slow = 0.0;
    int refractory = 2400, sinceOnset = 1 << 30, sinceNote = 1 << 30, onsetCount = 0;

    double unit = 0.0, fade = 1.0, fadeStep = 0.005;
    int tapOld = 768, tapNew = 768;
    uint64_t rng = 1;

    /** Transient on a 300 Hz high-passed copy: the fast envelope jumping well above the slow one.
        Through each note's attack (the refractory time) the slow envelope holds the peak, so a note's
        own decay or a held note never counts as a new one; only a fresh rise does. */
    bool detect (double x) noexcept
    {
        hp = hpCoeff * (hp + x - xPrev);
        xPrev = x;
        const double r = std::abs (hp);
        fast += (r > fast ? attack : release) * (r - fast);
        slow += slowCoeff * (fast - slow);
        if (sinceOnset < refractory)
        {
            slow = std::max (slow, fast);
            return false;
        }
        if (fast < kFloor || fast < kOnsetRatio * slow)
            return false;
        sinceOnset = 0;
        ++onsetCount;
        return true;
    }

    void moveTo (int tap) noexcept
    {
        if (fade < 1.0)
            tapOld = tapNew; // the refractory period is longer than a fade, so this only trims a fade's end
        if (tap == tapNew)
            return;
        tapOld = tapNew;
        tapNew = tap;
        fade = 0.0;
    }

    double read (int delay) const noexcept
    {
        const size_t n = buffer.size();
        return buffer[(write + n - (size_t) delay) % n];
    }

    double uniform() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return (double) (rng >> 11) * (1.0 / 9007199254740992.0);
    }
};
} // namespace rrd
