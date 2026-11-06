"""Frequency response of the 920-16 send/receive equalizers, transcribed from schematic D11688.

Both equalizers sit in the feedback loop of a UA 1109 card (non-inverting op-amp core):
pin 9 = EQ FEED (output side), pin 10 = EQ RETURN (inverting input), pin 8 = EQ GND.
Transformer step-up/down is a frequency-flat constant here and is normalized out.

Run:  python3 ctc_eq_networks.py [--plot out.png]
"""
from __future__ import annotations

import argparse

import numpy as np

from ctc_reference_model import Tube, cooper_loss_db

U, N, M, K = 1e-6, 1e-9, 1e-3, 1e3


def par(*z: np.ndarray) -> np.ndarray:
    return 1.0 / sum(1.0 / zi for zi in z)


def zc(c: float, w: np.ndarray) -> np.ndarray:
    return 1.0 / (1j * w * c)


def zl(l: float, w: np.ndarray) -> np.ndarray:
    return 1j * w * l


def send_gain(f: np.ndarray, mid_range_ohm: float = 2.5 * K) -> np.ndarray:
    """Channel A SEND (B identical): G = 1 + Zf/Zg.

    Zf (pin9->pin10): R2 3.9k || C7 1nF
    Zg (pin10->pin8): [L1 25mH + C4 9.4nF + R6 120] || [C5 68nF + R7 120]
                      || [R3 1.8k + R4 (0..5k "MID RANGE") + (R5 10k || C6 0.15uF)]
    """
    w = 2 * np.pi * f
    zf = par(3.9 * K + 0 * w, zc(1 * N, w))
    b1 = zl(25 * M, w) + zc(9.4 * N, w) + 120
    b2 = zc(68 * N, w) + 120
    b3 = 1.8 * K + mid_range_ohm + par(10 * K + 0 * w, zc(0.15 * U, w))
    return 1 + zf / par(b1, b2, b3)


def send_output_filter(f: np.ndarray, r_driver: float = 200.0) -> np.ndarray:
    """R1 120 ohm series + C3 0.1uF shunt into the ~200 ohm R-50 driver (output-transformer source ~0)."""
    w = 2 * np.pi * f
    z_load = par(zc(0.1 * U, w), r_driver + 0 * w)
    return z_load / (120 + z_load)


def receive_gain(f: np.ndarray, lf_ohm: float = 1.0 * K, hf_ohm: float = 500.0) -> np.ndarray:
    """Channel A RECEIVE (B identical). T-network feedback with a series-resonant shunt.

    pin9 -R16 560- Z ; Z -[(R18 47k||C16 0.1u) + (R17 18k||C15 470p)]- pin10
    Z -[L3 25mH + C17 9.4nF + R23 0..500 "10KC"]- pin8
    pin10 -[C13 22nF + R20 120]- pin8 ; pin10 -[R19 1.5k + (C14 0.22u || (R21 0..2k "L.F." + R22 560))]- pin8
    With V10 = Vin = 1 (virtual short): Vz = 1 + Zr/Zg ; Vout = Vz + R16*((Vz-1)/Zr + Vz/Zsh).
    """
    w = 2 * np.pi * f
    zr = par(47 * K + 0 * w, zc(0.1 * U, w)) + par(18 * K + 0 * w, zc(470e-12, w))
    zsh = zl(25 * M, w) + zc(9.4 * N, w) + hf_ohm
    zg = par(zc(22 * N, w) + 120, 1.5 * K + par(zc(0.22 * U, w), lf_ohm + 560 + 0 * w))
    vz = 1 + zr / zg
    return vz + 560 * ((vz - 1) / zr + vz / zsh)


def db(x: np.ndarray) -> np.ndarray:
    return 20 * np.log10(np.abs(x))


def norm_1k(f: np.ndarray, h: np.ndarray) -> np.ndarray:
    return db(h) - np.interp(1000.0, f, db(h))


def report() -> None:
    f = np.array([31.5, 50, 100, 200, 500, 700, 1e3, 2e3, 3e3, 5e3, 7e3, 1e4, 1.2e4, 1.5e4, 2e4])
    print("freq_hz        ", " ".join(f"{x:>7.0f}" for x in f))
    for label, h in [
        ("send mid=0", send_gain(f, 0)),
        ("send mid=2.5k", send_gain(f, 2.5 * K)),
        ("send mid=5k", send_gain(f, 5 * K)),
        ("send out RC", send_output_filter(f)),
        ("rx lf=0 hf=0", receive_gain(f, 0, 0)),
        ("rx lf=1k hf=500", receive_gain(f, 1 * K, 500)),
        ("rx lf=2k hf=500", receive_gain(f, 2 * K, 500)),
    ]:
        print(f"{label:<15}", " ".join(f"{x:>7.1f}" for x in norm_1k(f, h)))

    ff = np.geomspace(20, 20000, 4000)
    e = send_gain(ff) * send_output_filter(ff) * receive_gain(ff)
    for name, tau in (("A", 0.016), ("B", 0.014)):
        t = Tube(tau)
        ideal = Tube(tau, diameter=t.diameter, loss_scale=1.0)
        for law, loss in (("kirchhoff", db(ideal.response(ff))),
                          ("cooper", -cooper_loss_db(ff, t.length, t.diameter))):
            net = norm_1k(ff, e) + loss - np.interp(1000.0, ff, loss)
            sel = (ff >= 40) & (ff <= 10000)
            print(f"EQ x tube {name} {law:<9}: 40Hz-10kHz span {net[sel].min():+.1f}..{net[sel].max():+.1f} dB; "
                  f"@10k {np.interp(1e4, ff, net):+.1f}, @15k {np.interp(1.5e4, ff, net):+.1f}, "
                  f"@20k {np.interp(2e4, ff, net):+.1f}")
    # If the calibrated line is flat (factory chart: ~±0.7 dB to ~11 kHz), the two transducers + cavities
    # must supply the inverse of (EQ x tube). Print that implied combined transducer curve.
    t = Tube(0.016)
    loss = -cooper_loss_db(f, t.length, t.diameter)
    implied = -(norm_1k(f, send_gain(f) * send_output_filter(f) * receive_gain(f))
                + loss - (-cooper_loss_db(1000.0, t.length, t.diameter)))
    print("implied xducer A", " ".join(f"{x:>7.1f}" for x in implied))

    sel = (ff >= 20) & (ff <= 20000)
    pk = ff[sel][np.argmax(db(e)[sel])]
    print(f"total electronic EQ peak at {pk:.0f} Hz, "
          f"{db(e)[sel].max() - np.interp(1000.0, ff, db(e)):+.1f} dB re 1 kHz")


def plot(path: str) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    f = np.geomspace(20, 20000, 4000)
    fig, ax = plt.subplots(figsize=(9, 5), constrained_layout=True)
    for m in (0, 2.5 * K, 5 * K):
        ax.semilogx(f, norm_1k(f, send_gain(f, m) * send_output_filter(f)), label=f"Send EQ, MID RANGE={m/K:.1f}k")
    for lf, hf in ((0, 0), (1 * K, 500), (2 * K, 500)):
        ax.semilogx(f, norm_1k(f, receive_gain(f, lf, hf)), "--", label=f"Receive EQ, L.F.={lf/K:.0f}k, 10KC={hf:.0f}")
    e = send_gain(f) * send_output_filter(f) * receive_gain(f)
    ax.semilogx(f, norm_1k(f, e), "k", lw=2, label="Send x Receive (nominal trims)")
    t = Tube(0.016)
    loss = -cooper_loss_db(f, t.length, t.diameter)
    ax.semilogx(f, norm_1k(f, e) + loss - np.interp(1000.0, f, loss), "r", lw=2,
                label=f"x Ch A tube loss (Cooper law, {t.diameter * 1000:.1f} mm)")
    ax.set_xlabel("Hz")
    ax.set_ylabel("dB re 1 kHz")
    ax.set_ylim(-30, 40)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)
    ax.set_title("920-16 electronic EQ from schematic D11688 (transducers not included)")
    fig.savefig(path, dpi=130)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--plot", default=None)
    args = ap.parse_args()
    report()
    if args.plot:
        plot(args.plot)
