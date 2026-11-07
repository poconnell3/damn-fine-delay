"""Spec tests for the Red Room Delay golden-reference engine (doc 03 section 7)."""
from __future__ import annotations

from dataclasses import replace

import numpy as np
import pytest

import ctc_eq_networks as ref_eq
from analog import analog_fir, receive_network, send_network, send_output_rc
from engine import (A_NOMINAL, NOISE_BW_HZ, NOMINAL_DBFS, TAU_A, TAU_B, RedRoomDelay, Settings,
                    Trim, db, digital_delay, group_delay_1k)
from ctc_reference_model import Tube

FS = 48000


PRE = 0.05          # impulse position (s); keeps it clear of the resampler's edge


def impulse(n_s: float = 0.5, fs: int = FS) -> np.ndarray:
    x = np.zeros(int((n_s + PRE) * fs))
    x[int(PRE * fs)] = 1.0
    return x


def spectrum(y: np.ndarray, fs: int = FS) -> tuple[np.ndarray, np.ndarray]:
    return np.fft.rfftfreq(len(y), 1.0 / fs), np.fft.rfft(y)


def rel_db(f: np.ndarray, h: np.ndarray) -> np.ndarray:
    m = db(h)
    return m - np.interp(1000.0, f, m)


def tone(f: float, dbu: float, dur: float = 1.0, fs: int = FS) -> np.ndarray:
    t = np.arange(int(dur * fs)) / fs
    return 10.0 ** ((dbu - 4.0 + NOMINAL_DBFS) / 20.0) * np.sqrt(2.0) * np.sin(2.0 * np.pi * f * t)


def harmonics(y: np.ndarray, f0: float, fs: int = FS, t0: float = 0.4, dur: float = 0.5) -> np.ndarray:
    seg = y[int(t0 * fs): int(t0 * fs) + int(dur * fs)]
    mag = np.abs(np.fft.rfft(seg)) * 2.0 / len(seg)
    k = int(round(f0 * dur))
    return np.array([mag[k * h] for h in range(1, 8) if k * h < len(mag)])


def thd_pct(h: np.ndarray) -> float:
    return float(np.sqrt(np.sum(h[1:] ** 2)) / h[0] * 100.0)


@pytest.fixture(scope="module")
def linear_b() -> RedRoomDelay:
    return RedRoomDelay(FS, Settings(color="B").linear())


@pytest.fixture(scope="module")
def linear_a() -> RedRoomDelay:
    return RedRoomDelay(FS, Settings(color="A").linear())


@pytest.mark.parametrize("fs_os", [88200, 96000, 192000])
def test_eq_networks_match_reference(fs_os: int) -> None:
    f = np.geomspace(20.0, 16000.0, 300)
    for rat, ref in ((send_network(), ref_eq.send_gain(f)),
                     (send_output_rc(), ref_eq.send_output_filter(f)),
                     (receive_network(), ref_eq.receive_gain(f))):
        h = analog_fir(rat, fs_os, 1 << int(np.ceil(np.log2(4096 * fs_os / 96000))))
        n = np.arange(len(h))
        got = np.exp(-2j * np.pi * np.outer(f, n) / fs_os) @ h
        err = rel_db(f, got) - rel_db(f, ref)
        assert np.max(np.abs(err)) < 0.1


@pytest.mark.parametrize("fs", [44100, 48000, 88200, 96000])
def test_delay_accuracy(fs: int) -> None:
    eng = RedRoomDelay(fs, replace(Settings(color="B").linear(), echo=False))
    for line, tau in (("a", TAU_A), ("b", TAU_B)):
        y = getattr(eng, f"wet_{line}")(impulse(1.0, fs))
        assert abs(group_delay_1k(y, fs) - PRE - tau) * fs < 0.05


def test_echo_position_and_level() -> None:
    on = RedRoomDelay(FS, Settings(color="B").linear())
    off = RedRoomDelay(FS, replace(Settings(color="B").linear(), echo=False))
    for line, tau in (("a", TAU_A), ("b", TAU_B)):
        direct = getattr(off, f"wet_{line}")(impulse())
        echo = getattr(on, f"wet_{line}")(impulse()) - direct
        peak_t = np.argmax(np.abs(echo)) / FS - PRE
        assert abs(peak_t - 3.0 * tau) < 0.5e-3
        f, d = spectrum(direct)
        _, e = spectrum(echo)
        tube = Tube(tau)
        for fq in (100.0, 1000.0):
            measured = db(np.interp(fq, f, np.abs(e))) - db(np.interp(fq, f, np.abs(d)))
            model = db(0.09 * np.abs(tube.response(np.array([fq]), 2))[0])
            assert abs(measured - model) < 2.0


def test_ripple_spacing(linear_b: RedRoomDelay) -> None:
    for line, tau in (("a", TAU_A), ("b", TAU_B)):
        y = getattr(linear_b, f"wet_{line}")(impulse(1.0))
        f, h = spectrum(y)
        band = (f > 30.0) & (f < 4000.0)
        mag = db(h[band])
        mag -= np.convolve(mag, np.ones(201) / 201, "same")
        n_fft = 8 * (len(mag) - 400)
        cep = np.abs(np.fft.rfft(mag[200:-200] * np.hanning(len(mag) - 400), n_fft))
        quef = np.fft.rfftfreq(n_fft, f[1] - f[0])                    # cycles per Hz = seconds
        sel = (quef > 0.01) & (quef < 0.06)
        period_hz = 1.0 / quef[sel][np.argmax(cep[sel])]
        assert abs(period_hz - 1.0 / (2.0 * tau)) / (1.0 / (2.0 * tau)) < 0.01


def test_cascade_beat_null() -> None:
    """The two first echoes (58 and 62 ms) beat; the null sits near 1/(2*4 ms) = 125 Hz, pulled to ~120 Hz
    by the extra wall-loss phase of the longer A tube."""
    on = RedRoomDelay(FS, Settings(color="B").linear())
    off = RedRoomDelay(FS, replace(Settings(color="B").linear(), echo=False))
    echo = on.cascade(impulse(1.0)) - off.cascade(impulse(1.0))
    f, e = spectrum(echo)
    sel = (f > 80.0) & (f < 200.0)
    measured = f[sel][np.argmin(np.abs(e[sel]))]

    ff = np.arange(80.0, 200.0, 0.1)
    ha, hb = Tube(TAU_A).response(ff), Tube(TAU_B).response(ff)
    w = 2.0 * np.pi * ff
    model = (ha ** 3 * hb * np.exp(-1j * w * 2 * TAU_A) + ha * hb ** 3 * np.exp(-1j * w * 2 * TAU_B))
    expected = ff[np.argmin(np.abs(model))]
    assert 115.0 < expected < 125.0
    assert abs(measured - expected) < 2.0


def test_color_b_frequency_response(linear_b: RedRoomDelay) -> None:
    for line in ("a", "b"):
        f, h = spectrum(getattr(linear_b, f"wet_{line}")(impulse(1.0)))
        m = rel_db(f, h)
        band = (f >= 40.0) & (f <= 10000.0)
        assert np.max(np.abs(m[band])) <= 0.7
        assert np.abs(db(np.interp(1000.0, f, np.abs(h)))) < 0.2           # unity gain at 1 kHz
        assert np.interp(12000.0, f, m) < -6.0
        assert np.interp(15000.0, f, m) < -20.0


def test_color_a_within_factory_tolerance(linear_a: RedRoomDelay) -> None:
    f, h = spectrum(linear_a.wet_a(impulse(1.0)))
    m = rel_db(f, h)
    band = (f >= 40.0) & (f <= 10000.0)
    assert np.max(np.abs(m[band])) <= 2.0
    assert np.interp(7000.0, f, m) < -0.7                                # residual "icicle" notch present


def test_trims_change_tone() -> None:
    bright = RedRoomDelay(FS, replace(Settings(color="B").linear(), trim_a=Trim(hf_ohm=200.0)))
    f, h = spectrum(bright.wet_a(impulse(1.0)))
    assert np.interp(10000.0, f, rel_db(f, h)) > 1.0


def test_thd_at_nominal() -> None:
    eng = RedRoomDelay(FS, Settings(noise="off", crosstalk=False))
    assert thd_pct(harmonics(eng.wet_a(tone(1000.0, 4.0)), 1000.0)) <= 0.5
    for fq in (100.0, 2000.0, 5000.0, 8000.0):
        assert thd_pct(harmonics(eng.wet_a(tone(fq, 4.0)), fq)) < 1.0


def test_distortion_rises_with_level() -> None:
    eng = RedRoomDelay(FS, Settings(noise="off", crosstalk=False))
    low = thd_pct(harmonics(eng.wet_a(tone(100.0, 4.0)), 100.0))
    hot = thd_pct(harmonics(eng.wet_a(tone(100.0, 16.0)), 100.0))
    assert low < 0.1 < 3.0 < hot


def test_noise_floor() -> None:
    eng = RedRoomDelay(FS, Settings(crosstalk=False))
    y = eng.wet_a(np.zeros(3 * FS))[FS:]
    f, h = spectrum(y)
    band = f <= NOISE_BW_HZ
    rms = np.sqrt(np.sum(np.abs(h[band]) ** 2) * 2.0) / len(y)
    sn = NOMINAL_DBFS - db(rms)
    assert 70.0 <= sn <= 76.0


def test_crosstalk() -> None:
    eng = RedRoomDelay(FS, replace(Settings(color="B").linear(), crosstalk=True))
    for fq, spec in ((1000.0, -45.0), (10000.0, -25.0)):
        x = tone(fq, 4.0)
        leak_a, _ = eng.dual(np.zeros_like(x), x)
        level = db(harmonics(leak_a, fq)[0] / harmonics(x, fq)[0])
        assert abs(level - spec) < 1.5


def test_routings_and_digital_reference() -> None:
    eng = RedRoomDelay(FS, Settings(color="B").linear())
    x = impulse()
    wide = eng.widener(x)
    assert wide.shape == (len(x), 2)
    start = int(PRE * FS)
    assert abs(np.argmax(np.abs(wide[:, 0])) - start - TAU_A * FS) <= 1
    assert abs(np.argmax(np.abs(wide[:, 1])) - start - TAU_B * FS) <= 1
    assert abs(np.argmax(np.abs(eng.cascade(x))) - start - (TAU_A + TAU_B) * FS) <= 2
    dbl = eng.doubler(x)
    assert dbl[start] == pytest.approx(1.0, abs=1e-3)
    assert np.argmax(digital_delay(x, FS, TAU_A)) == start + round(TAU_A * FS)


def test_hot_input_stays_finite() -> None:
    eng = RedRoomDelay(FS, Settings(drive_db=20.0, noise="extra"))
    rng = np.random.default_rng(0)
    y = eng.widener(np.clip(rng.standard_normal(FS), -1.0, 1.0))
    assert np.all(np.isfinite(y))
    assert np.max(np.abs(y)) < 10.0
    assert np.max(np.abs(eng.wet_a(A_NOMINAL * np.ones(FS // 4)))) < 1.0   # DC is blocked, no runaway
