"""Null test: real-time C++ engine vs the Python golden reference (research/prototype/engine.py).

Linear core (nonlinearities, noise and crosstalk off), impulse responses at 48 kHz. Pass criteria:
  - complex error < -45 dB from 40 Hz to 2 kHz (relative to the 1 kHz level): same delay, echo and tone
    (-39 dB for the cascade, where two lines in series double the deviation);
  - magnitude difference within +-0.1 dB from 40 Hz to 10 kHz.

Above ~2 kHz the two realizations differ slightly in phase near the 11 kHz band edge. The engine maps the
band-edge low-pass with the bilinear transform; the reference builds it as a minimum-phase FIR that only sees
the response up to its Nyquist frequency. Against the exact analog target at 9 kHz the engine is off by
0.03 rad at 4x and 0.11 rad at 2x, the reference by 0.09 rad. That is under 2 us of group delay, so it is
reported, not failed.

Run:  PYTHONPATH=<prototype dir> python3 tests/null_test.py <path to dfd_cli>
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from dataclasses import replace
from pathlib import Path

import numpy as np

from engine import DamnFineDelay, Settings, db

FS = 48000
SECONDS = 1.0
PRE = 2400


def cpp_ir(cli: str, os_factor: int, color: str, routing: str, echo: bool) -> np.ndarray:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "ir.f64"
        subprocess.run([cli, "ir", str(FS), str(os_factor), color, routing, "1" if echo else "0", str(SECONDS), str(out)],
                       check=True)
        return np.fromfile(out).reshape(2, -1)


def reference_ir(color: str, routing: str, echo: bool) -> np.ndarray:
    eng = DamnFineDelay(FS, replace(Settings(color=color).linear(), echo=echo))
    n = int(SECONDS * FS)
    x = np.zeros(n + PRE)
    x[PRE] = 1.0
    if routing == "cascade":
        y = eng.cascade(x)[PRE:]
        return np.stack([y, y])
    return np.stack([eng.wet_a(x)[PRE:], eng.wet_b(x)[PRE:]])


def compare(cpp: np.ndarray, ref: np.ndarray) -> tuple[float, float, float]:
    n = min(len(cpp), len(ref))
    f = np.fft.rfftfreq(n, 1.0 / FS)
    c, r = np.fft.rfft(cpp[:n]), np.fft.rfft(ref[:n])
    err = db(c - r) - db(np.interp(1000.0, f, np.abs(r)))
    low = (f >= 40.0) & (f <= 2000.0)
    band = (f >= 40.0) & (f <= 10000.0)
    high = (f > 2000.0) & (f <= 9000.0)
    mag = np.abs(db(c[band]) - db(r[band]))
    return float(err[low].max()), float(mag.max()), float(err[high].max())


def main(cli: str) -> int:
    failures = 0
    for os_factor in (2, 4):
        for color in ("B", "A"):
            for routing in ("dual", "cascade"):
                for echo in (False, True):
                    cpp = cpp_ir(cli, os_factor, color, routing, echo)
                    ref = reference_ir(color, routing, echo)
                    for i, name in enumerate(("A+B",) if routing == "cascade" else ("A", "B")):
                        low, mag, high = compare(cpp[i], ref[i])
                        ok = low < (-39.0 if routing == "cascade" else -45.0) and mag < 0.1
                        failures += not ok
                        print(f"{'ok  ' if ok else 'FAIL'} os {os_factor} color {color} {routing:7s} line {name:3s} "
                              f"echo {'on ' if echo else 'off'}: error {low:6.1f} dB (40 Hz-2 kHz), "
                              f"max level diff {mag:.3f} dB (40 Hz-10 kHz), [error 2-9 kHz {high:6.1f} dB]")
    print(f"{failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/dfd_cli"))
