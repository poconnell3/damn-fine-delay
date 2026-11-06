"""Generate src/dsp/GeneratedFilters.h from the 920-16 schematic and the tube physics.

- SEND and RECEIVE networks (schematic D11688) reduced symbolically to minimal order (5 and 6), with the
  trimmer resistances (R4 MID RANGE, R21 L.F., R23 10KC) left as variables. Coefficients are in the
  normalized variable p = s / (2*pi*1 kHz).
- Wall-loss filter H_w = exp(-(1+j) alpha L) of each tube, fitted as a cascade of first-order analog shelves
  (Abel/Smyth/Smith, DAFx-03 style): minimum phase, unity DC gain.

Run from the repository root:  python3 tools/gen_filters.py
Needs numpy, scipy, sympy, and research/tools/ctc_reference_model.py on PYTHONPATH (for the tube model).
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import sympy as sp
from scipy import optimize

from ctc_reference_model import Tube

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "src" / "dsp" / "GeneratedFilters.h"
W_N = 2.0 * np.pi * 1000.0
TRIM_RANGES = {"mid": (0.0, 5000.0), "lf": (0.0, 2000.0), "hf": (0.0, 500.0)}
N_SHELVES = 12
FIT_BAND = (5.0, 30000.0)
TOP_SHELF_HZ = 50000.0

p = sp.symbols("p")
r_mid, r_lf, r_hf = sp.symbols("rMid rLf rHf", nonnegative=True)


def _r(v):
    return sp.Float(v) if not isinstance(v, sp.Basic) else v


def _c(farads):
    return 1 / (p * sp.Float(farads * W_N))


def _l(henries):
    return p * sp.Float(henries * W_N)


def _par(*zs):
    return 1 / sum(1 / z for z in zs)


def send_tf():
    zf = _par(_r(3900.0), _c(1e-9))
    zg = _par(_l(25e-3) + _c(9.4e-9) + _r(120.0), _c(68e-9) + _r(120.0),
              _r(1800.0) + r_mid + _par(_r(10000.0), _c(0.15e-6)))
    return sp.cancel(sp.together(1 + zf / zg))


def receive_tf():
    zr = _par(_r(47000.0), _c(0.1e-6)) + _par(_r(18000.0), _c(470e-12))
    zsh = _l(25e-3) + _c(9.4e-9) + r_hf
    zg = _par(_c(22e-9) + _r(120.0), _r(1500.0) + _par(_c(0.22e-6), r_lf + 560))
    vz = 1 + zr / zg
    return sp.cancel(sp.together(vz + 560 * ((vz - 1) / zr + vz / zsh)))


def coeffs(expr):
    """num/den coefficient lists (highest power of p first), normalized so den[0] has no common scale issues."""
    num, den = sp.fraction(expr)
    pn, pd = sp.Poly(sp.expand(num), p), sp.Poly(sp.expand(den), p)
    return pn.all_coeffs(), pd.all_coeffs()


def eval_coeffs(cs, subs):
    return np.array([float(c.subs(subs)) for c in cs])


def check_networks(send, receive):
    """Cross-check against the independent numpy implementation and confirm minimum phase over the trim box."""
    import ctc_eq_networks as ref

    f = np.geomspace(20.0, 20000.0, 200)
    jp = 1j * f / 1000.0
    for name, expr, ref_fn, trims in (
        ("send", send, lambda ff, t: ref.send_gain(ff, t["mid"]), ("mid",)),
        ("receive", receive, lambda ff, t: ref.receive_gain(ff, t["lf"], t["hf"]), ("lf", "hf")),
    ):
        n, d = coeffs(expr)
        grid = [dict(zip(trims, v)) for v in np.array(np.meshgrid(*[np.linspace(*TRIM_RANGES[t], 6) for t in trims]))
                .reshape(len(trims), -1).T]
        worst_db, worst_zero, worst_pole = 0.0, -np.inf, -np.inf
        for t in grid:
            subs = {r_mid: t.get("mid", 0.0), r_lf: t.get("lf", 0.0), r_hf: max(t.get("hf", 0.0), 1.0)}
            nn, dd = eval_coeffs(n, subs), eval_coeffs(d, subs)
            h = np.polyval(nn, jp) / np.polyval(dd, jp)
            t_ref = {**t, "hf": max(t.get("hf", 0.0), 1.0)}
            worst_db = max(worst_db, np.max(np.abs(20 * np.log10(np.abs(h / ref_fn(f, t_ref))))))
            worst_zero = max(worst_zero, np.max(np.roots(nn).real))
            worst_pole = max(worst_pole, np.max(np.roots(dd).real))
        print(f"{name}: order {len(n) - 1}/{len(d) - 1}, max |err| vs reference {worst_db:.2e} dB, "
              f"max Re(zero) {worst_zero:.3g}, max Re(pole) {worst_pole:.3g}")
        if worst_db > 1e-6 or worst_zero >= 0.0 or worst_pole >= 0.0:
            sys.exit(f"{name}: network check failed")


def fit_wall_loss(tau: float) -> tuple[np.ndarray, np.ndarray, float]:
    """Least-squares fit of log|H_w| with N first-order shelves (zero above pole, so each one is a min-phase cut).

    Only the magnitude is fitted. The phase then follows by minimum phase, apart from a near-constant group-delay
    offset (the fit stops growing above the fit band); that offset is a pure delay and is absorbed by the bulk-delay
    calibration, so the check below looks at the group-delay error relative to its value at 1 kHz.
    """
    tube = Tube(tau)
    f = np.geomspace(*FIT_BAND, 400)
    w2 = (2.0 * np.pi * f)[None, :] ** 2
    target = 20.0 * np.log10(np.abs(tube.response(f)))
    n = N_SHELVES

    def unpack(x):
        poles = np.exp(x[:n])
        return poles * np.exp(np.abs(x[n:])), poles

    def resid(x):
        z, q = (v[:, None] for v in unpack(x))
        return (10.0 * np.log10((w2 + z * z) / (w2 + q * q)) + 20.0 * np.log10(q / z)).sum(axis=0) - target

    x0 = np.concatenate([np.log(np.geomspace(2 * np.pi * 10.0, 2 * np.pi * TOP_SHELF_HZ, n)), np.full(n, 0.5)])
    sol = optimize.least_squares(resid, x0, xtol=1e-12, ftol=1e-12, max_nfev=3000, x_scale="jac")
    zeros, poles = unpack(sol.x)
    order = np.argsort(poles)
    zeros, poles = zeros[order], poles[order]

    fc = np.geomspace(20.0, 20000.0, 2000)
    w = 2.0 * np.pi * fc
    h = np.prod([(1j * w + z) / (1j * w + q) * q / z for z, q in zip(zeros, poles)], axis=0)
    ref = tube.response(fc)
    mag_err = np.max(np.abs(20.0 * np.log10(np.abs(h / ref))))
    gd = lambda hh: -np.gradient(np.unwrap(np.angle(hh)), w)
    gd_err = gd(h) - gd(ref)
    gd_spread = np.max(np.abs(gd_err - np.interp(1000.0, fc, gd_err)))
    print(f"tube {tau * 1e3:.0f} ms: {n} shelves, magnitude error 20 Hz-20 kHz {mag_err:.4f} dB, "
          f"group-delay error {np.interp(1000.0, fc, gd_err) * 1e6:+.1f} us at 1 kHz, spread {gd_spread * 1e6:.1f} us")
    if mag_err > 0.05 or gd_spread > 40e-6:
        sys.exit("wall-loss fit out of tolerance")
    return zeros, poles, float(np.abs(tube.response(np.array([1000.0])))[0])


def cpp_expr(e) -> str:
    return sp.ccode(sp.N(sp.expand(e), 17))


def emit_poly_fn(name: str, expr, args: str) -> str:
    n, d = coeffs(expr)
    lines = [f"inline Rational {name}({args})", "{", "    Rational r;"]
    lines.append("    r.num = { " + ", ".join(cpp_expr(c) for c in n) + " };")
    lines.append("    r.den = { " + ", ".join(cpp_expr(c) for c in d) + " };")
    lines += ["    return r;", "}"]
    return "\n".join(lines)


def emit_array(values) -> str:
    return "{ " + ", ".join(f"{v:.17g}" for v in values) + " }"


def main() -> None:
    send, receive = send_tf(), receive_tf()
    check_networks(send, receive)
    tubes = {"A": fit_wall_loss(0.016), "B": fit_wall_loss(0.014)}

    parts = [
        "// Generated by tools/gen_filters.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include <array>",
        "#include <vector>",
        "",
        "namespace dfd::generated",
        "{",
        "/** Polynomial coefficients, highest power first, in p = s / (2*pi*1000). */",
        "struct Rational",
        "{",
        "    std::vector<double> num, den;",
        "};",
        "",
        "constexpr double kNormRadPerSec = 6283.1853071795862;",
        "",
        "/** SEND network: G = 1 + Zf/Zg. rMid = R4 MID RANGE (0..5k). */",
        emit_poly_fn("sendNetwork", send, "double rMid"),
        "",
        "/** RECEIVE T-network. rLf = R21 L.F. (0..2k), rHf = R23 10KC (0..500, use >= 1 ohm). */",
        emit_poly_fn("receiveNetwork", receive, "double rLf, double rHf"),
        "",
        "/** First-order wall-loss shelves (s + z)/(s + p) * p/z, rad/s, and |H_w(1 kHz)| for gain makeup. */",
        "struct WallLossFit",
        "{",
        f"    std::array<double, {N_SHELVES}> zeros, poles;",
        "    double gainAt1k;",
        "};",
        "",
    ]
    for name, (zeros, poles, g1k) in tubes.items():
        parts.append(f"inline constexpr WallLossFit kWallLoss{name} {{ {emit_array(zeros)}, {emit_array(poles)}, {g1k:.17g} }};")
    parts += ["", "} // namespace dfd::generated", ""]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(parts))
    print(f"wrote {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
