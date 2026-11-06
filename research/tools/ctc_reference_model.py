"""Reference physics model of the UREI 920-16 Cooper Time Cube acoustic path.

Offline numpy prototype used to sanity-check the numbers in the project docs and to
serve as a golden reference when validating the real-time C++ implementation.

Run:  python3 ctc_reference_model.py [--plot out.png]
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass

import numpy as np

FT = 0.3048
INCH = 0.0254

# Air at 20 C, 1 atm
RHO = 1.204          # kg/m^3
MU = 1.81e-5         # Pa*s
GAMMA = 1.4
PRANDTL = 0.71


def speed_of_sound(temp_c: float) -> float:
    return 331.3 * np.sqrt(1.0 + temp_c / 273.15)


C0 = speed_of_sound(20.0)                  # ~343.2 m/s (Cooper used 343 m/s)
C_PATENT = 1130.0 * FT                     # 344.4 m/s (US 3,789,328)


def kirchhoff_alpha(f: np.ndarray, radius: float, c: float = C0) -> np.ndarray:
    """Wide-tube viscothermal attenuation in Np/m (Kirchhoff). ~3e-5*sqrt(f)/a."""
    omega = 2.0 * np.pi * np.asarray(f, dtype=float)
    nu = MU / RHO
    return (1.0 / (radius * c)) * np.sqrt(omega * nu / 2.0) * (1.0 + (GAMMA - 1.0) / np.sqrt(PRANDTL))


def cooper_loss_db(f: np.ndarray, length: float, diameter: float) -> np.ndarray:
    """Cooper's empirical tube loss, Audio Apr 1971 eq.(1): A = 0.026 * (L/d) * sqrt(f_kHz) dB."""
    return 0.026 * (length / diameter) * np.sqrt(np.asarray(f, dtype=float) / 1000.0)


@dataclass(frozen=True)
class Tube:
    delay_s: float
    # 12.7 mm (1/2"): photo estimate of the production poly tube (OD ~0.55-0.65" against 1" pegboard pitch);
    # Cooper's copper prototype was 14.3 mm. With loss_scale 1.6 (Cooper's measured law) this makes the
    # schematic EQ x tube imply a near-flat R50+R70 pair from 500 Hz to 7 kHz (see doc 07).
    diameter: float = 0.5 * INCH
    loss_scale: float = 1.6                # 1.0 = ideal Kirchhoff; 1.6 = Cooper's measured A = 0.026 (L/d) sqrt(f_kHz)

    @property
    def length(self) -> float:
        return self.delay_s * C0

    @property
    def radius(self) -> float:
        return self.diameter / 2.0

    def response(self, f: np.ndarray, passes: int = 1) -> np.ndarray:
        """Complex wall-loss filter H_w for `passes` traversals, bulk delay L/c0 removed.

        Wide-tube limit: the excess real wavenumber equals the attenuation, so
        H_w = exp(-(1+j) * alpha * L) per pass (minimum phase, sqrt(f) law).
        """
        alpha = self.loss_scale * kirchhoff_alpha(f, self.radius)
        return np.exp(-(1.0 + 1.0j) * alpha * self.length * passes)


@dataclass(frozen=True)
class Channel:
    tube: Tube
    r_receiver: float = 0.1    # residual reflection at receiver/stub junction; 0.1 fits the factory chart (<~1 dB p-p LF ripple)
    r_source: float = 0.9      # reflection off the back-loaded driver end (tunable, unmeasured)

    def transfer(self, f: np.ndarray, equalize: str = "minphase") -> np.ndarray:
        """Bulk-delayed transfer: direct arrival at tau + first echo at 3*tau.

        equalize:
          "minphase"  - ideal min-phase inverse of one tube pass (cancels loss AND dispersion
                        of the direct path; approximates a well-matched analog pre/post EQ)
          "magnitude" - flatten magnitude only (leaves direct-path dispersion)
          "none"      - raw acoustic path
        """
        omega = 2.0 * np.pi * f
        tau = self.tube.delay_s
        direct = self.tube.response(f, 1) * np.exp(-1j * omega * tau)
        echo = (self.r_receiver * self.r_source * self.tube.response(f, 3)
                * np.exp(-1j * omega * 3.0 * tau))
        h = direct + echo
        if equalize == "minphase":
            h = h / self.tube.response(f, 1)
        elif equalize == "magnitude":
            h = h / np.abs(self.tube.response(f, 1))
        elif equalize != "none":
            raise ValueError(f"unknown equalize mode: {equalize}")
        return h

    def impulse_response(self, fs: float, n: int, equalize: str = "minphase") -> np.ndarray:
        """Real IR of `transfer` via inverse rFFT (for null tests against the C++ engine)."""
        f = np.fft.rfftfreq(n, 1.0 / fs)
        f[0] = 1e-3  # avoid sqrt(0)/division issues at DC; wall loss -> 0 there anyway
        return np.fft.irfft(self.transfer(f, equalize), n)


def excess_delays(tube: Tube, f: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Excess phase and group delay (s) of the wall-loss filter relative to L/c0."""
    alpha_l = tube.loss_scale * kirchhoff_alpha(f, tube.radius) * tube.length
    omega = 2.0 * np.pi * f
    phase_delay = alpha_l / omega
    group_delay = alpha_l / (2.0 * omega)   # d/dw of alpha*L with alpha ~ sqrt(w)
    return phase_delay, group_delay


def ripple_db(h: np.ndarray) -> float:
    mag = 20.0 * np.log10(np.abs(h))
    return float(mag.max() - mag.min())


def nonlinear_distortion_pct(particle_velocity: float, tau: float, f: float) -> float:
    """Cooper eq.(4): cumulative waveform steepening, D = 0.5(1+gamma) v T (2 pi / lambda) * 100%."""
    wavelength = C0 / f
    return 0.5 * (1.0 + GAMMA) * particle_velocity * tau * (2.0 * np.pi / wavelength) * 100.0


def report() -> dict:
    out: dict = {}
    a = Channel(Tube(0.016))
    b = Channel(Tube(0.014))

    out["patent_lengths_ft"] = {"14ms": 0.014 * C_PATENT / FT, "16ms": 0.016 * C_PATENT / FT}
    out["model_lengths_m"] = {"A": a.tube.length, "B": b.tube.length}

    f_pts = np.array([100.0, 1000.0, 10000.0])
    for name, ch in (("A", a), ("B", b)):
        kirch = 8.686 * kirchhoff_alpha(f_pts, ch.tube.radius) * ch.tube.length
        coop = cooper_loss_db(f_pts, ch.tube.length, ch.tube.diameter)
        out[f"loss_db_{name}"] = {"kirchhoff": kirch.round(1).tolist(), "cooper": coop.round(1).tolist()}
        pd, gd = excess_delays(ch.tube, f_pts)
        out[f"excess_delay_us_{name}"] = {"phase": (pd * 1e6).round(0).tolist(),
                                          "group": (gd * 1e6).round(0).tolist()}

    # Cooper's own 16 ft / 14.3 mm prototype, to check his published numbers (2.8 / 8.8 / 27.8 dB)
    out["cooper_prototype_db"] = cooper_loss_db(f_pts, 16 * FT, 9 / 16 * INCH).round(1).tolist()

    out["ratio_cooper_over_kirchhoff"] = float(
        cooper_loss_db(1000.0, 1.0, 0.0143) / (8.686 * kirchhoff_alpha(1000.0, 0.0143 / 2)))

    # Reflection ripple after ideal equalization, by band
    f = np.linspace(40.0, 10000.0, 200000)
    for name, ch in (("A", a), ("B", b)):
        h = ch.transfer(f)
        bands = {}
        for lo, hi in ((40, 200), (200, 1000), (1000, 3000), (3000, 10000)):
            m = (f >= lo) & (f < hi)
            bands[f"{lo}-{hi}Hz"] = round(ripple_db(h[m]), 2)
        out[f"ripple_pp_db_{name}"] = bands
        out[f"ripple_spacing_hz_{name}"] = 1.0 / (2.0 * ch.tube.delay_s)

    # Echo level relative to direct (after EQ) at a few freqs
    for name, ch in (("A", a), ("B", b)):
        rel = ch.r_receiver * ch.r_source * np.abs(ch.tube.response(f_pts, 2))
        out[f"echo_rel_db_{name}"] = (20 * np.log10(rel)).round(1).tolist()

    # Cascade (A -> B): echoes at 2*tauA and 2*tauB after the 30 ms main arrival
    out["cascade"] = {"main_ms": 30.0, "echo_ms": [30.0 + 28.0, 30.0 + 32.0],
                      "echo_spacing_ms": 4.0, "beat_null_first_hz": 1.0 / (2 * 0.004)}

    out["stub_quarter_wave_hz"] = C_PATENT / (4.0 * 51 * INCH)

    t_lo, t_hi = 15.0, 30.0
    out["delay_A_vs_temp_ms"] = {f"{t_lo}C": 1e3 * a.tube.length / speed_of_sound(t_lo),
                                  f"{t_hi}C": 1e3 * a.tube.length / speed_of_sound(t_hi)}

    out["nonlinear_D_pct_at_5k_nominal"] = nonlinear_distortion_pct(6.5e-3, 0.0141, 5000.0)

    fs = 48000.0
    ir = a.impulse_response(fs, 8192)
    main_idx = int(np.argmax(np.abs(ir)))
    tail = np.abs(ir[main_idx + int(0.02 * fs):])
    echo_idx = main_idx + int(0.02 * fs) + int(np.argmax(tail))
    out["ir_A_48k"] = {"main_ms": 1e3 * main_idx / fs, "echo_ms": 1e3 * echo_idx / fs,
                       "echo_minus_main_ms": 1e3 * (echo_idx - main_idx) / fs}
    return out


def plot(path: str) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    f = np.geomspace(20.0, 20000.0, 8000)
    a = Channel(Tube(0.016))
    b = Channel(Tube(0.014))
    fig, axes = plt.subplots(2, 1, figsize=(9, 7), constrained_layout=True)

    ax = axes[0]
    ideal = Tube(0.016, diameter=a.tube.diameter, loss_scale=1.0)
    ax.semilogx(f, 20 * np.log10(np.abs(ideal.response(f))), label="Ch A raw tube loss (ideal Kirchhoff)")
    ax.semilogx(f, -cooper_loss_db(f, a.tube.length, a.tube.diameter), "--",
                label="Ch A raw tube loss (Cooper empirical, model default)")
    ax.set_ylabel("dB")
    ax.set_title(f"Unequalized tube loss, {a.tube.diameter * 1000:.1f} mm bore, 16 ms line")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()

    ax = axes[1]
    ax.semilogx(f, 20 * np.log10(np.abs(a.transfer(f))), lw=0.8, label="Ch A 16 ms, equalized + echo")
    ax.semilogx(f, 20 * np.log10(np.abs(b.transfer(f))) - 6, lw=0.8, label="Ch B 14 ms (offset -6 dB)")
    ax.set_ylabel("dB")
    ax.set_xlabel("Hz")
    ax.set_title("Residual ripple from the 3-tau echo (r_rx=0.1, r_src=0.9)")
    ax.set_ylim(-10, 3)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.savefig(path, dpi=130)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--plot", default=None)
    args = ap.parse_args()
    for k, v in report().items():
        print(f"{k}: {v}")
    if args.plot:
        plot(args.plot)
