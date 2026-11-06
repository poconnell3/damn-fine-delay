"""Exact analog transfer functions of the 920-16 EQ networks and their digital realizations.

Impedances are built as rational functions of a frequency-normalized Laplace variable
p = s / W_N (W_N = 2*pi*1 kHz) to keep polynomial coefficients well conditioned.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy import signal

W_N = 2.0 * np.pi * 1000.0


@dataclass(frozen=True)
class Rational:
    """num(p)/den(p) with numpy poly1d coefficients (highest power first)."""

    num: np.poly1d
    den: np.poly1d

    @staticmethod
    def const(v: float) -> "Rational":
        return Rational(np.poly1d([v]), np.poly1d([1.0]))

    def __add__(self, o: "Rational") -> "Rational":
        return Rational(self.num * o.den + o.num * self.den, self.den * o.den)

    def __sub__(self, o: "Rational") -> "Rational":
        return Rational(self.num * o.den - o.num * self.den, self.den * o.den)

    def __mul__(self, o: "Rational") -> "Rational":
        return Rational(self.num * o.num, self.den * o.den)

    def __truediv__(self, o: "Rational") -> "Rational":
        return Rational(self.num * o.den, self.den * o.num)

    def __call__(self, f_hz: np.ndarray) -> np.ndarray:
        p = 1j * 2.0 * np.pi * np.asarray(f_hz, dtype=float) / W_N
        return self.num(p) / self.den(p)


def R(ohms: float) -> Rational:
    return Rational.const(ohms)


def C(farads: float) -> Rational:
    return Rational(np.poly1d([1.0]), np.poly1d([farads * W_N, 0.0]))


def L(henries: float) -> Rational:
    return Rational(np.poly1d([henries * W_N, 0.0]), np.poly1d([1.0]))


def par(*zs: Rational) -> Rational:
    one = Rational.const(1.0)
    acc = one / zs[0]
    for z in zs[1:]:
        acc = acc + one / z
    return one / acc


def send_network(mid_range_ohm: float = 2500.0) -> Rational:
    """SEND 1109 feedback EQ (schematic D11688): G = 1 + Zf/Zg."""
    zf = par(R(3.9e3), C(1e-9))
    zg = par(L(25e-3) + C(9.4e-9) + R(120.0),
             C(68e-9) + R(120.0),
             R(1.8e3) + R(max(mid_range_ohm, 1e-3)) + par(R(10e3), C(0.15e-6)))
    return Rational.const(1.0) + zf / zg


def send_output_rc(r_driver: float = 200.0) -> Rational:
    """R1 120 ohm series, C3 0.1 uF shunt, into the ~200 ohm R50."""
    z_load = par(C(0.1e-6), R(r_driver))
    return z_load / (R(120.0) + z_load)


def receive_network(lf_ohm: float = 1000.0, hf_ohm: float = 500.0) -> Rational:
    """RECEIVE 1109 T-network feedback EQ (schematic D11688)."""
    one = Rational.const(1.0)
    zr = par(R(47e3), C(0.1e-6)) + par(R(18e3), C(470e-12))
    zsh = L(25e-3) + C(9.4e-9) + R(max(hf_ohm, 1.0))   # >=1 ohm: coil DCR keeps the resonance finite
    zg = par(C(22e-9) + R(120.0), R(1.5e3) + par(C(0.22e-6), R(max(lf_ohm, 0.0) + 560.0)))
    vz = one + zr / zg
    return vz + R(560.0) * ((vz - one) / zr + vz / zsh)


def analog_fir(rat: Rational, fs: float, n_taps: int = 4096, norm_hz: float | None = 1000.0,
               guard_lp_hz: float = 24000.0, guard_order: int = 8) -> np.ndarray:
    """Exact analog response sampled on an FFT grid -> FIR (golden-reference realization).

    A Butterworth guard low-pass (8th order) at `guard_lp_hz` (standing in for the 1109 and transformer
    bandwidth) removes the discontinuity at Nyquist so the impulse response stays causal; n_taps covers
    the slowest low-frequency poles so circular wrap-around is negligible.
    """
    f = np.fft.rfftfreq(n_taps, 1.0 / fs)
    f[0] = 1e-3
    h = rat(f)
    b, a = signal.butter(guard_order, 2.0 * np.pi * guard_lp_hz, analog=True)
    h = h * signal.freqs(b, a, 2.0 * np.pi * f)[1]
    if norm_hz is not None:
        h = h / np.abs(rat(np.array([norm_hz])))[0]
    return np.fft.irfft(h, n_taps)


def first_order_sos(b_s: tuple[float, float], a_s: tuple[float, float], fs: float, prewarp_hz: float) -> np.ndarray:
    """Digitize (b0 s + b1)/(a0 s + a1) given in rad/s with pre-warping."""
    w0 = 2.0 * np.pi * prewarp_hz
    k = w0 / np.tan(w0 / (2.0 * fs))
    b, a = signal.bilinear([b_s[0], b_s[1]], [a_s[0], a_s[1]], fs=k / 2.0)
    return signal.tf2sos(b, a)
