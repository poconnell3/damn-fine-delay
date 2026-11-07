# Red Room Delay: Python prototype (golden reference)

Offline engine for the 920-16 signal path. It is the reference the C++ plugin gets null-tested against, and it renders the listening demos.

## Run

From `research/prototype/` (the engine imports the physics models in `research/tools/`):

```
PYTHONPATH=../tools python3 -m pytest -q test_engine.py     # 19 spec tests
PYTHONPATH=../tools python3 render.py out                   # WAV + MP3 demos, plots, metrics.json
```

Needs numpy, scipy, soundfile, matplotlib, pytest. ffmpeg is optional (MP3s).

## Files

| File | What it does |
|---|---|
| `analog.py` | Exact analog transfer functions of the SEND, output RC and RECEIVE networks (schematic D11688) as rational functions, realized as FIRs on an FFT grid with an 8th-order 24 kHz guard low-pass. Error ≤ 0.007 dB from 20 Hz to 16 kHz. |
| `engine.py` | `RedRoomDelay` (lines A and B, routings) and `Line` (one tube path). |
| `signals.py` | Synthesized test program: Karplus-Strong guitar, formant voice, drum loop. |
| `render.py` | Demo renders, response plots, metrics. |
| `test_engine.py` | Spec tests from doc 03 §7. |

## Signal chain (per line, 2× oversampled)

1. Drive gain.
2. Input transformer: flux shelf (4/40 Hz) → `12·tanh(x/12)` → exact inverse shelf.
3. SEND EQ FIR, then output RC FIR (trimmer R4 MID RANGE is a parameter).
4. Driver excursion limit: excursion = 2nd-order 200 Hz low-pass of the drive; asymmetric soft knee (+ side strikes the cover plate at a 100 Hz, +10 dBu input; − side 1.5× further); the correction is added back to the drive.
5. Transducer pair (R50 + R70) as a minimum-phase FIR. Its magnitude is the **implied** curve: the inverse of factory-trimmed EQ × tube loss, normalized at 1 kHz, times the band-edge low-pass (8th-order Chebyshev I, 0.1 dB ripple, 11 kHz). Color A adds the unit residuals (7 kHz −1 dB Q6, 9 kHz −0.6 dB Q5, 3 kHz +0.5 dB Q1.5). A 2nd-order 15 Hz high-pass (stub and leaks) runs as an IIR after it.
6. Air nonlinearity: `y = x + k·d(x²)/dt`.
7. Tube: wall loss `exp(−(1+j)αL)` (Kirchhoff × 1.6, 12.7 mm bore), bulk delay, and the first echo at 3τ (r = 0.1 × 0.9) with three passes of loss. Gain makeup 1/|H_w(1 kHz)|.
8. Pickup noise (white, at the receive input).
9. RECEIVE EQ FIR (trimmers R21 L.F. and R23 10KC are parameters).
10. Output transformer, then 1/drive.

Crosstalk: a 20 dB/decade leak from the other line's input, −45 dB at 1 kHz.

## Calibration choices

- **Delay.** The bulk delay is trimmed so the realized chain reads exactly τ at 1 kHz. The trim is measured from the realized FIRs with a linear phase fit over 800–1200 Hz. A ±1 Hz difference is not enough: the long low-level tails (the √f loss kernel, the 15 Hz high-pass) put a fine ripple on the phase that biased the first version by about 5 µs. The tube IR uses ≥ (3τ + 250 ms) of FFT length for the same reason.
- **Noise.** 72 dB S/N re nominal (−18 dBFS = +4 dBu) in 15.7 kHz bandwidth. Assumes the spec's ">70 dB" is referenced to nominal, not maximum, output. The noise level inside the unit is fixed, so raising Drive (with the output trimmed back) lowers the hiss.
- **Air NL.** `k = 0.00076 / (A_nominal·2π·1 kHz)`. Because the send pre-emphasis raises HF level in the tube and the receive EQ boosts the 2nd harmonic, output THD peaks near 5 kHz. This k puts that peak at 0.85% at +4 dBu, inside the spec's <1%. 1 kHz: 0.05%.
- **Cascade beat null** sits at ~120 Hz, not 1/(2·4 ms) = 125 Hz: the A tube's extra wall-loss phase shifts it. The test compares against the physics model.

## Measured (48 kHz)

| Check | Target | Measured |
|---|---|---|
| Delay at 1 kHz (44.1/48/96 kHz) | ± 0.05 samples | ≤ 0.0001 samples |
| Echo time | 3τ ± 0.5 ms | 48.3 / 42.3 ms |
| Echo level, A | model ± 2 dB | −28.0 dB @ 100 Hz, −43.4 dB @ 1 kHz |
| Ripple spacing | 1/(2τ) ± 1% | pass |
| Cascade null | model ± 2 Hz | 120 Hz |
| Color B, 40 Hz–10 kHz | ± 0.7 dB | −0.52…+0.42 dB |
| Above band | steep | −7.7 dB @ 12 kHz, −35 dB @ 15 kHz |
| Color A | ± 2 dB | −1.0…+0.6 dB |
| THD +4 dBu | < 0.5% @ 1 kHz, < 1% elsewhere | 0.05% @ 1 kHz, 0.85% @ 5 kHz |
| Driver strike, 100 Hz | onset ~+10 dBu | 0.01% @ +4, 1.8% @ +10, 12.6% @ +16 dBu |
| S/N | 70–76 dB | 72.0 dB |
| Crosstalk | −45 / −25 dB | −45.3 / −25.3 dB |

## Not in this prototype

Feedback / Extended mode, Temperature affecting loss (it changes delay only), Unit seeds (one fixed set of residuals), Tuning, real-time block processing. The FIRs here are the accuracy reference; the real-time C++ engine (`plugin/`) uses IIRs and is null-tested against this folder (`plugin/tests/null_test.py`).
