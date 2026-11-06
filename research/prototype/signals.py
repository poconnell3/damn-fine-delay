"""Synthesized test program: plucked guitar, vowel-singing voice, and a drum loop (all mono, float64)."""
from __future__ import annotations

import numpy as np
from scipy import signal

FS = 48000


def _midi(n: float) -> float:
    return 440.0 * 2.0 ** ((n - 69) / 12.0)


def _to_level(x: np.ndarray, rms_dbfs: float = -18.0, peak_dbfs: float = -1.0) -> np.ndarray:
    active = np.abs(x) > 1e-3 * np.max(np.abs(x))
    x = x * 10.0 ** (rms_dbfs / 20.0) / np.sqrt(np.mean(x[active] ** 2))
    peak = np.max(np.abs(x))
    limit = 10.0 ** (peak_dbfs / 20.0)
    return x * min(1.0, limit / peak)


def _pluck(f0: float, dur: float, fs: int, rng: np.random.Generator, bright: float = 0.6) -> np.ndarray:
    """Karplus-Strong string with a fractional-delay loop and a pick-position comb."""
    n = int(dur * fs)
    period = fs / f0
    d = int(period - 0.6)                          # two-point average adds half a sample
    frac = period - 0.5 - d                        # in [0.1, 1.1): keeps the allpass stable and smooth
    eta = (1.0 - frac) / (1.0 + frac)
    excite = rng.uniform(-1.0, 1.0, d)
    excite = signal.lfilter([bright, 1.0 - bright], [1.0], excite)
    pick = int(0.13 * d)
    excite[pick:] -= 0.9 * excite[:-pick] if pick else 0.0
    buf = np.zeros(n + d + 2)
    buf[:d] = excite
    ap_x1 = ap_y1 = 0.0
    loss = 0.996
    for i in range(d, n + d):
        avg = 0.5 * (buf[i - d] + buf[i - d - 1]) if i - d - 1 >= 0 else 0.5 * buf[i - d]
        ap = eta * avg + ap_x1 - eta * ap_y1
        ap_x1, ap_y1 = avg, ap
        buf[i] = loss * ap
    return buf[d:n + d]


def guitar(fs: int = FS, seed: int = 7) -> np.ndarray:
    rng = np.random.default_rng(seed)
    chords = [(40, 47, 52, 55, 59, 64), (48, 52, 55, 60, 64, 67), (43, 47, 50, 55, 59, 67), (50, 57, 62, 66, 69, 74)]
    beat = 0.36
    out = np.zeros(int(fs * (len(chords) * 8 * beat + 2.5)))
    pattern = (0, 3, 4, 5, 2, 4, 3, 4)
    t = 0.0
    for chord in chords:
        for step, s in enumerate(pattern):
            note = _pluck(_midi(chord[s]), 2.2, fs, rng, bright=0.5 + 0.2 * rng.random())
            note *= 0.8 if step else 1.0
            i = int(t * fs)
            out[i:i + len(note)] += note
            t += beat
    body = signal.tf2sos(*signal.iirpeak(110.0, 3.0, fs))
    out = out + 0.6 * signal.sosfilt(body, out)
    for f0, gain, q in ((220.0, 0.5, 2.0), (2500.0, 0.3, 1.0)):
        out += gain * signal.lfilter(*signal.iirpeak(f0, q, fs), out)
    return _to_level(out)


def _glottal(f0: np.ndarray, fs: int) -> np.ndarray:
    """Rosenberg-style glottal flow derivative from a phase accumulator."""
    phase = np.cumsum(f0 / fs) % 1.0
    open_q, close_q = 0.45, 0.17
    flow = np.where(phase < open_q, 0.5 * (1.0 - np.cos(np.pi * phase / open_q)),
                    np.where(phase < open_q + close_q, np.cos(0.5 * np.pi * (phase - open_q) / close_q), 0.0))
    return np.diff(flow, prepend=0.0)


VOWELS = {
    "a": ((800, 80), (1150, 90), (2900, 120), (3900, 130)),
    "e": ((400, 70), (1600, 80), (2700, 100), (3300, 120)),
    "i": ((300, 60), (2300, 100), (3000, 120), (3700, 150)),
    "o": ((450, 70), (800, 80), (2830, 100), (3500, 130)),
    "u": ((325, 60), (700, 70), (2530, 100), (3500, 130)),
}


def voice(fs: int = FS, seed: int = 3) -> np.ndarray:
    rng = np.random.default_rng(seed)
    melody = [(57, "a", 0.7), (60, "o", 0.5), (62, "e", 0.5), (64, "a", 1.0), (62, "i", 0.5),
              (60, "o", 0.5), (59, "u", 0.6), (57, "a", 1.4)]
    total = sum(d for *_, d in melody) + 1.0
    n = int(total * fs)
    t = np.arange(n) / fs
    f0 = np.zeros(n)
    amp = np.zeros(n)
    formants = np.zeros((n, 4, 2))
    pos = 0
    for note, vowel, dur in melody:
        m = int(dur * fs)
        seg = slice(pos, pos + m)
        f0[seg] = _midi(note)
        env = np.minimum(1.0, np.arange(m) / (0.04 * fs)) * np.minimum(1.0, (m - np.arange(m)) / (0.06 * fs))
        amp[seg] = env
        formants[seg] = VOWELS[vowel]
        pos += m
    glide = int(0.05 * fs)
    f0[pos:] = f0[pos - 1]
    formants[pos:] = formants[pos - 1]
    kernel = np.hanning(glide) / np.hanning(glide).sum()
    f0 = np.convolve(f0, kernel, mode="same")
    f0[: glide] = f0[glide]
    for k in range(4):
        for j in range(2):
            formants[:, k, j] = np.convolve(formants[:, k, j], np.hanning(4 * glide) / np.hanning(4 * glide).sum(), "same")
    vib = 1.0 + 0.012 * np.sin(2.0 * np.pi * 5.4 * t) * np.minimum(1.0, t % 1.0 * 2.0)
    src = _glottal(f0 * vib, fs) * amp + 0.004 * rng.standard_normal(n) * amp
    # Time-varying cascade of two-pole resonators, coefficients updated per 64-sample block.
    y = src
    block = 64
    for k in range(4):
        out = np.zeros(n)
        z = np.zeros(2)
        for b0 in range(0, n, block):
            fc, bw = formants[min(b0 + block // 2, n - 1), k]
            r = np.exp(-np.pi * bw / fs)
            a1, a2 = -2.0 * r * np.cos(2.0 * np.pi * fc / fs), r * r
            seg = y[b0:b0 + block]
            out[b0:b0 + block], z = signal.lfilter([1.0 + a1 + a2], [1.0, a1, a2], seg, zi=z)
        y = out
    y = signal.lfilter([1.0, -0.95], [1.0], y)                   # lip radiation
    return _to_level(y)


def drums(fs: int = FS, bpm: float = 100.0, bars: int = 4, seed: int = 11) -> np.ndarray:
    rng = np.random.default_rng(seed)
    step = 60.0 / bpm / 4.0
    n = int((bars * 16 * step + 1.5) * fs)
    out = np.zeros(n)

    def place(sound: np.ndarray, at: float, gain: float = 1.0) -> None:
        i = max(0, int(at * fs))
        m = min(len(sound), n - i)
        out[i:i + m] += gain * sound[:m]

    tk = np.arange(int(0.45 * fs)) / fs
    kick = np.sin(2.0 * np.pi * np.cumsum(45.0 + 75.0 * np.exp(-tk / 0.035)) / fs) * np.exp(-tk / 0.16)
    kick[: int(0.002 * fs)] += rng.uniform(-0.6, 0.6, int(0.002 * fs))
    ts = np.arange(int(0.3 * fs)) / fs
    snare_noise = signal.sosfilt(signal.butter(2, (1500, 9000), "bandpass", fs=fs, output="sos"),
                                 rng.standard_normal(len(ts)))
    snare = 0.6 * np.sin(2.0 * np.pi * 185.0 * ts) * np.exp(-ts / 0.05) + 0.8 * snare_noise * np.exp(-ts / 0.09)
    th = np.arange(int(0.12 * fs)) / fs
    hat_sos = signal.butter(4, 7000, "highpass", fs=fs, output="sos")

    kicks = (0, 7, 8, 10)
    snares = (4, 12)
    for bar in range(bars):
        t0 = bar * 16 * step
        for s in range(16):
            at = t0 + s * step
            if s in kicks:
                place(kick, at, 1.0 if s != 7 else 0.6)
            if s in snares:
                place(snare, at)
            if s % 2 == 0:
                hat = signal.sosfilt(hat_sos, rng.standard_normal(len(th))) * np.exp(-th / (0.02 if s % 4 else 0.035))
                place(hat, at + 0.004 * rng.standard_normal(), 0.35 if s % 4 else 0.5)
    return _to_level(out, rms_dbfs=-20.0)


ALL = {"guitar": guitar, "voice": voice, "drums": drums}
