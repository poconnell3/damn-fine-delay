"""Damn Fine Delay - offline golden-reference engine (Python prototype of the 920-16 signal path).

Per line, at the oversampled rate:
    drive -> input transformer -> SEND EQ -> output RC -> driver excursion limit
    -> transducer pair (R50 + R70) -> air nonlinearity -> tube (wall loss, delay, echo at 3 tau)
    -> pickup noise -> RECEIVE EQ -> output transformer -> 1/drive
plus a high-pass-shaped crosstalk leak between the A and B lines.

Level convention: -18 dBFS RMS = +4 dBu (the unit's nominal level).
"""
from __future__ import annotations

from dataclasses import dataclass, field, replace

import numpy as np
from scipy import signal

from analog import analog_fir, first_order_sos, receive_network, send_network, send_output_rc
from ctc_reference_model import C0, Tube, speed_of_sound

NOMINAL_DBFS = -18.0
A_NOMINAL = 10.0 ** (NOMINAL_DBFS / 20.0) * np.sqrt(2.0)          # peak of a nominal sine
TAU_A, TAU_B = 0.016, 0.014
CUTOFF_HZ = 11000.0                                                # top of the calibrated band
CUTOFF_ORDER = 8
CUTOFF_RIPPLE_DB = 0.1
NOISE_BW_HZ = 15700.0
SN_AUTHENTIC_DB = 72.0
NOISE_LEVELS_DB = {"off": None, "authentic": 0.0, "extra": 12.0}


def db(x: np.ndarray) -> np.ndarray:
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-300))


@dataclass(frozen=True)
class Trim:
    """Positions of the three EQ trimmers (R4 MID RANGE, R21 L.F., R23 10KC)."""

    mid_range_ohm: float = 2500.0
    lf_ohm: float = 1000.0
    hf_ohm: float = 500.0


# Residual faults of a well-calibrated unit (doc 06 section 4): (Hz, dB, Q).
UNIT_RESIDUALS: tuple[tuple[float, float, float], ...] = (
    (7000.0, -1.0, 6.0),    # "icicle" notch, special 99H350B diaphragm
    (9000.0, -0.6, 5.0),    # slight ~9 kHz hole
    (3000.0, 0.5, 1.5),     # 3 kHz compensator residual
)


@dataclass(frozen=True)
class Settings:
    color: str = "A"                    # "A" = original with residuals, "B" = leveled
    drive_db: float = 0.0
    noise: str = "authentic"            # off / authentic / extra
    nonlinear: bool = True
    echo: bool = True
    temp_c: float = 20.0
    trim_a: Trim = field(default_factory=Trim)
    trim_b: Trim = field(default_factory=Trim)
    residuals: tuple[tuple[float, float, float], ...] = UNIT_RESIDUALS
    crosstalk: bool = True
    seed: int = 1

    def linear(self) -> "Settings":
        return replace(self, nonlinear=False, noise="off", crosstalk=False)


def peaking_mag(f: np.ndarray, f0: float, gain_db: float, q: float) -> np.ndarray:
    """|H| of an analog constant-Q bell."""
    a = 10.0 ** (gain_db / 40.0)
    w, w0 = 2.0 * np.pi * f, 2.0 * np.pi * f0
    re = w0 * w0 - w * w
    return np.abs((re + 1j * w * w0 * a / q) / (re + 1j * w * w0 / (a * q)))


def cutoff_db(f: np.ndarray) -> np.ndarray:
    """Top of the calibrated band: 8th-order Chebyshev I low-pass, 0.1 dB ripple, edge at CUTOFF_HZ."""
    b, a = signal.cheby1(CUTOFF_ORDER, CUTOFF_RIPPLE_DB, 2.0 * np.pi * CUTOFF_HZ, analog=True)
    return db(signal.freqs(b, a, 2.0 * np.pi * np.asarray(f, dtype=float))[1])


def minimum_phase_fir(mag_db_fn, fs: float, n_taps: int, n_fft: int) -> np.ndarray:
    """Minimum-phase FIR for a magnitude (dB) via the folded real cepstrum."""
    f = np.abs(np.fft.fftfreq(n_fft, 1.0 / fs))
    f[0] = 1e-3
    log_mag = np.maximum(mag_db_fn(f), -160.0) * (np.log(10.0) / 20.0)
    cep = np.fft.ifft(log_mag).real
    fold = np.zeros(n_fft)
    fold[0] = cep[0]
    fold[1:n_fft // 2] = 2.0 * cep[1:n_fft // 2]
    fold[n_fft // 2] = cep[n_fft // 2]
    h = np.fft.ifft(np.exp(np.fft.fft(fold))).real[:n_taps]
    fade = n_taps // 8
    h[-fade:] *= 0.5 * (1.0 + np.cos(np.linspace(0.0, np.pi, fade)))
    return h


def dtft(h: np.ndarray, f: np.ndarray, fs: float) -> np.ndarray:
    n = np.arange(len(h))
    return np.exp(-2j * np.pi * np.outer(np.atleast_1d(f), n) / fs) @ h


def group_delay_1k(h: np.ndarray, fs: float) -> float:
    """Group delay (s) at 1 kHz from a linear phase fit over 800-1200 Hz.

    A +-1 Hz difference is not enough: long low-level tails (the sqrt(f) loss kernel, the 15 Hz high-pass) put a
    fine ripple on the phase that biases a narrow estimate by several microseconds.
    """
    f = np.linspace(800.0, 1200.0, 401)               # 1 Hz steps keep the unwrap valid up to ~0.5 s of delay
    phase = np.unwrap(np.angle(signal.zoom_fft(h, [800.0, 1200.0], m=401, fs=fs, endpoint=True)))
    return -float(np.polyfit(f, phase, 1)[0]) / (2.0 * np.pi)


def _fftconv(x: np.ndarray, h: np.ndarray) -> np.ndarray:
    return signal.oaconvolve(x, h)[: len(x)]


def _pow2(n: float) -> int:
    return 1 << int(np.ceil(np.log2(max(n, 2))))


def _knee(e: np.ndarray, limit: float, knee: float = 0.7) -> np.ndarray:
    a, b = knee * limit, (1.0 - knee) * limit
    return np.where(e <= a, e, a + b * np.tanh((e - a) / b))


class Transformer:
    """Flux saturation: integrate-like shelf -> soft limit -> exact inverse shelf."""

    def __init__(self, fs: float, sat: float = 12.0, f_low: float = 4.0, f_high: float = 40.0):
        wl, wh = 2.0 * np.pi * f_low, 2.0 * np.pi * f_high
        self.flux = first_order_sos((1.0, wh), (1.0, wl), fs, prewarp_hz=f_high)
        self.inverse = first_order_sos((1.0, wl), (1.0, wh), fs, prewarp_hz=f_high)
        self.sat = sat

    def __call__(self, x: np.ndarray) -> np.ndarray:
        phi = signal.sosfilt(self.flux, x)
        return signal.sosfilt(self.inverse, self.sat * np.tanh(phi / self.sat))


class Line:
    """One acoustic line (A or B) running at the oversampled rate."""

    def __init__(self, tau: float, fs_os: float, settings: Settings, trim: Trim, seed: int):
        self.tau_nominal = tau
        self.fs = fs_os
        self.settings = settings
        self.tube = Tube(tau)
        self.tau = tau * C0 / speed_of_sound(settings.temp_c)
        n_eq = _pow2(4096 * fs_os / 96000.0)

        self.send = analog_fir(send_network(trim.mid_range_ohm), fs_os, n_eq)
        self.rc = analog_fir(send_output_rc(), fs_os, n_eq)
        self.receive = analog_fir(receive_network(trim.lf_ohm, trim.hf_ohm), fs_os, n_eq)
        self.xducer = self._transducer_fir(n_eq * 2)
        self.stub_hp = signal.butter(2, 15.0, "highpass", fs=fs_os, output="sos")   # stub/leak LF limit
        self.tube_ir = self._tube_ir()

        self.in_xfmr = Transformer(fs_os)
        self.out_xfmr = Transformer(fs_os)
        self.excursion = signal.butter(2, 200.0, fs=fs_os, output="sos")
        self.limit_pos, self.limit_neg = self._excursion_limits()
        # H2 rises with f and with the HF pre-emphasis; this puts the worst case (~5 kHz) at ~0.9 % THD
        # at +4 dBu, inside the <1 % spec (1 kHz: ~0.05 %).
        self.k_air = 0.00076 / (A_NOMINAL * 2.0 * np.pi * 1000.0)
        self.noise_sigma = self._noise_sigma()
        self.rng = np.random.default_rng(seed)

    # ----- calibration -------------------------------------------------------------------------
    def _transducer_db(self, f: np.ndarray) -> np.ndarray:
        """Implied R50+R70 pair: the inverse of factory-trimmed EQ x tube, times the band-edge low-pass."""
        factory = Trim()
        eq = (send_network(factory.mid_range_ohm) * send_output_rc()
              * receive_network(factory.lf_ohm, factory.hf_ohm))
        ref = np.array([1000.0])

        def chain_db(ff: np.ndarray) -> np.ndarray:
            return db(eq(ff)) + db(self.tube.response(ff))

        out = -(chain_db(f) - chain_db(ref)[0]) + cutoff_db(f) - cutoff_db(ref)[0]
        if self.settings.color == "A":
            for f0, g, q in self.settings.residuals:
                out += db(peaking_mag(f, f0, g, q))
        return out

    def _transducer_fir(self, n_taps: int) -> np.ndarray:
        h = minimum_phase_fir(self._transducer_db, self.fs, n_taps, n_taps * 4)
        return h / np.abs(dtft(h, 1000.0, self.fs))[0]

    def _group_delay_1k(self, h: np.ndarray) -> float:
        return group_delay_1k(h, self.fs)

    def _tube_ir(self) -> np.ndarray:
        """Wall loss + bulk delay + first echo, with the bulk delay trimmed so the whole line reads tau at 1 kHz."""
        n = _pow2((3.0 * self.tau + 0.25) * self.fs)              # long: the sqrt(f) loss kernel has a t^-1.5 tail
        f = np.fft.rfftfreq(n, 1.0 / self.fs)
        f[0] = 1e-3
        w = 2.0 * np.pi * f
        direct = self.tube.response(f, 1)
        echo = 0.1 * 0.9 * self.tube.response(f, 3) * np.exp(-2j * w * self.tau)
        norm = np.abs(self.tube.response(np.array([1000.0])))[0]
        hp = signal.sosfilt(self.stub_hp, np.r_[1.0, np.zeros(n - 1)])
        others = sum(self._group_delay_1k(np.r_[h, np.zeros(n)]) for h in (self.send, self.rc, self.xducer,
                                                                          self.receive, hp))
        bulk = self.tau - others
        for _ in range(3):
            got = self._group_delay_1k(np.fft.irfft(direct * np.exp(-1j * w * bulk), n)) + others
            bulk += self.tau - got
        h = direct + echo if self.settings.echo else direct
        return np.fft.irfft(h * np.exp(-1j * w * bulk), n) / norm

    def _upstream_gain(self, f_hz: float) -> float:
        return float(np.abs(dtft(self.send, f_hz, self.fs) * dtft(self.rc, f_hz, self.fs))[0])

    def _excursion_limits(self) -> tuple[float, float]:
        """R50 strikes its cover plate at 100 Hz, +10 dBm at the input."""
        a_in = 10.0 ** ((10.0 - 4.0 + NOMINAL_DBFS) / 20.0) * np.sqrt(2.0)
        _, h = signal.sosfreqz(self.excursion, worN=[100.0], fs=self.fs)
        e = a_in * self._upstream_gain(100.0) * abs(h[0])
        return e, 1.5 * e

    def _noise_sigma(self) -> float:
        """White noise at the receive input giving 72 dB S/N re nominal in 15.7 kHz at the output."""
        h = np.fft.rfft(self.receive, 1 << 16)
        f = np.fft.rfftfreq(1 << 16, 1.0 / self.fs)
        band = f <= NOISE_BW_HZ
        gain2 = np.sum(np.abs(h[band]) ** 2) / (len(f) - 1)       # fraction of white-noise power
        target_rms = 10.0 ** ((NOMINAL_DBFS - SN_AUTHENTIC_DB) / 20.0)
        return target_rms / np.sqrt(gain2)

    # ----- processing --------------------------------------------------------------------------
    def _driver(self, x: np.ndarray) -> np.ndarray:
        e = signal.sosfilt(self.excursion, x)
        hit = np.where(e >= 0.0, _knee(e, self.limit_pos), -_knee(-e, self.limit_neg))
        return x + (hit - e)

    def _air(self, x: np.ndarray) -> np.ndarray:
        return x + self.k_air * self.fs * np.gradient(x * x)

    def process(self, x: np.ndarray) -> np.ndarray:
        s = self.settings
        g = 10.0 ** (s.drive_db / 20.0)
        y = x * g
        if s.nonlinear:
            y = self.in_xfmr(y)
        y = _fftconv(_fftconv(y, self.send), self.rc)
        if s.nonlinear:
            y = self._driver(y)
        y = signal.sosfilt(self.stub_hp, _fftconv(y, self.xducer))
        if s.nonlinear:
            y = self._air(y)
        y = _fftconv(y, self.tube_ir)
        level = NOISE_LEVELS_DB[s.noise]
        if level is not None:
            y = y + self.rng.standard_normal(len(y)) * self.noise_sigma * 10.0 ** (level / 20.0)   # fixed noise; drive trims it
        y = _fftconv(y, self.receive)
        if s.nonlinear:
            y = self.out_xfmr(y)
        return y / g


class DamnFineDelay:
    """Two lines (A = 16 ms, B = 14 ms) with routings. All I/O at the base rate `fs`."""

    def __init__(self, fs: float = 48000.0, settings: Settings = Settings(), oversample: int = 2):
        self.fs, self.os, self.settings = fs, oversample, settings
        fs_os = fs * oversample
        self.a = Line(TAU_A, fs_os, settings, settings.trim_a, settings.seed)
        self.b = Line(TAU_B, fs_os, settings, settings.trim_b, settings.seed + 1)
        # Feed B, read A: -45 dB @ 1 kHz, -25 dB @ 10 kHz (a 20 dB/decade leak).
        wc = 2.0 * np.pi * 40000.0
        g = 10.0 ** (-45.0 / 20.0) * abs(1j * 2.0 * np.pi * 1000.0 + wc) / (2.0 * np.pi * 1000.0)
        self.leak = first_order_sos((g, 0.0), (1.0, wc), fs_os, prewarp_hz=10000.0)

    def _up(self, x: np.ndarray) -> np.ndarray:
        return signal.resample_poly(x, self.os, 1, window=("kaiser", 10.0))

    def _down(self, x: np.ndarray) -> np.ndarray:
        return signal.resample_poly(x, 1, self.os, window=("kaiser", 10.0))

    def dual(self, in_a: np.ndarray, in_b: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """Both lines, independent inputs (the hardware's normal mode), wet outputs."""
        ua, ub = self._up(in_a), self._up(in_b)
        ya, yb = self.a.process(ua), self.b.process(ub)
        if self.settings.crosstalk:
            ya = ya + signal.sosfilt(self.leak, ub)
            yb = yb + signal.sosfilt(self.leak, ua)
        return self._down(ya), self._down(yb)

    def cascade(self, x: np.ndarray) -> np.ndarray:
        """A output into B input: 30 ms."""
        return self._down(self.b.process(self.a.process(self._up(x))))

    def wet_a(self, x: np.ndarray) -> np.ndarray:
        return self.dual(x, np.zeros_like(x))[0]

    def wet_b(self, x: np.ndarray) -> np.ndarray:
        return self.dual(np.zeros_like(x), x)[1]

    def widener(self, x: np.ndarray) -> np.ndarray:
        """Mono in, A to the left, B to the right (wet only, as in Authentic mode)."""
        return np.stack(self.dual(x, x), axis=1)

    def doubler(self, x: np.ndarray, wet_db: float = -3.0) -> np.ndarray:
        """Dry plus line A 3 dB under it (the manual's doubling tip)."""
        return x + 10.0 ** (wet_db / 20.0) * self.wet_a(x)


def digital_delay(x: np.ndarray, fs: float, tau: float) -> np.ndarray:
    n = int(round(tau * fs))
    return np.concatenate([np.zeros(n), x[: len(x) - n]])
