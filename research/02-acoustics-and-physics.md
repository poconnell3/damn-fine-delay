# 02: Acoustics and Physics of the Time Cube

All numbers marked *(model)* come from `tools/ctc_reference_model.py`. Current defaults: **12.7 mm bore** (photo estimate, doc 07), **Cooper loss law (1.6× Kirchhoff)**, 20 °C air, c₀ = 343.2 m/s. Tables labelled "14.3 mm" use the earlier assumption, Cooper's prototype bore, and are kept for comparison.

## 1. Delay

τ = L / c. The patent's 1,130 ft/s gives 15.82 ft for 14 ms and 18.08 ft for 16 ms. The model reproduces both exactly. At c₀ = 343.2 m/s the lengths are 4.81 m (B) and 5.49 m (A).

**Sample counts:**

| Rate | 14 ms | 16 ms | 30 ms |
|---|---|---|---|
| 44.1 kHz | 617.4 | 705.6 | 1323.0 |
| 48 kHz | 672 | 768 | 1440 |
| 96 kHz | 1344 | 1536 | 2880 |

At 44.1 kHz the delays are fractional, so we need a fractional-delay read. The 0.4–0.6 sample error matters because the comb-filter notch positions against the dry signal depend on the exact delay.

## 2. Temperature

c ≈ 331.3·√(1 + T/273.15). Channel A runs from **16.14 ms at 15 °C to 15.73 ms at 30 °C** *(model)*, about −0.17%/°C. Room temperature drifts slowly, so this is a static trim, not modulation. The factory spec lists wow and flutter as 0%, so the authentic mode should have **no** modulation.

## 3. Wall (viscothermal) losses: the dominant filter

Sound moving down a narrow tube loses energy in thin viscous and thermal boundary layers at the wall. In the wide-tube (Kirchhoff) limit:

α(f) = (1 / (a·c)) · √(ω·ν / 2) · (1 + (γ−1)/√Pr)  ≈  **3.0×10⁻⁵ · √f / a**  [Np/m, a in m]

where a is the radius, ν the kinematic viscosity, γ = 1.4, and Pr = 0.71. Loss grows with √f and with length, and falls as the bore gets wider (1/a). This is the 3×10⁻⁵ formula JOS cites for cylindrical tubes.

**Cooper's empirical formula** (Audio, Apr 1971): **A_dB = 0.026 · (L/d) · √f_kHz**. It comes out **1.60× the ideal Kirchhoff value** *(model)*. Real tubes lose more than theory because of rough walls, joints, bends, and wall compliance. The model checks Cooper's published numbers for his 16 ft prototype: 2.8 / 8.8 / 27.8 dB at 100 Hz / 1 kHz / 10 kHz vs 2.8 / 8.9 / 28.1 *(model)*.

Unequalized loss for the production lines *(model)*:

| Line | Bore | 100 Hz | 1 kHz | 10 kHz | Formula |
|---|---|---|---|---|---|
| **A (16 ms, 5.49 m)** | **12.7 mm** | **3.6 dB** | **11.2 dB** | **35.6 dB** | **Cooper (default)** |
| **B (14 ms, 4.81 m)** | **12.7 mm** | **3.1** | **9.8** | **31.1** | **Cooper (default)** |
| A | 14.3 mm | 2.0 | 6.2 | 19.7 | Kirchhoff |
| A | 14.3 mm | 3.2 | 10.0 | 31.6 | Cooper |
| B | 14.3 mm | 1.7 | 5.5 | 17.3 | Kirchhoff |
| B | 14.3 mm | 2.8 | 8.7 | 27.7 | Cooper |

Two consequences follow:

- The raw tube is a gentle but deep low-pass filter, roughly −20 to −36 dB at 10 kHz depending on bore and loss law (about −36 dB at the current defaults). That is **why** the electronics use strong HF pre-emphasis.
- Channel A has about 10–15% more HF loss than B. **The two channels are not tonally identical.**

**Bandwidth after EQ.** The ±2 dB to 10 kHz spec is what is left once the EQ has made up 20–30 dB of HF loss. The UREI EQ does this with LC resonances tuned to **10.4 kHz** in both the send and receive amps (doc 06). Above that, the boost collapses while tube loss (−28 to −45 dB at 20 kHz) and transducer rolloff keep rising. The factory chart shows a **near-vertical cutoff at about 11–12 kHz**. This steep top end is part of why it "sits in the mix."

## 4. Dispersion

In the same wide-tube limit, the excess real wavenumber equals α. So low frequencies travel slightly slower:

Excess phase delay = α·L / ω; excess group delay = α·L / (2ω).

| Line A (Kirchhoff) | 100 Hz | 1 kHz | 10 kHz |
|---|---|---|---|
| Excess phase delay | 361 µs | 114 µs | 36 µs |
| Excess group delay | 181 µs | 57 µs | 18 µs |

(That table is for 14.3 mm with Kirchhoff loss. At the current defaults, 12.7 mm with Cooper loss, the raw tube's excess group delay is **325 / 103 / 33 µs** at 100 Hz / 1 kHz / 10 kHz.)

**Important nuance.** The wall-loss filter is minimum-phase, per Abel/Smyth/Smith (DAFx-03). A minimum-phase EQ that inverts its magnitude also inverts its phase. So a well-matched analog pre/post EQ largely **cancels** the dispersion too. Only the mismatch between Cooper/UREI's simple RC network and the true √f curve is left over, so the residual dispersion is a second-order effect. The *echo* path (section 5) is not equalized relative to the direct path, so it keeps its extra loss and dispersion.

## 5. Reflections: the "fast wiggles"

The patent's whole purpose is to stop the tube acting like a resonant pipe. The termination is imperfect, so a small echo survives. Dave Berners (UA) explains the plug-in's "wiggly" response like this:

- The direct sound arrives at **τ**.
- Some of it reflects at the receiver/stub junction, travels back, reflects off the driver end, and arrives again **2τ later** (at 3τ total: about 32 ms after the main pulse on A and 28 ms on B). Later reflections are negligible.
- The echo is **strongly low-pass filtered**, so the ripple shrinks at high frequencies.

Physics explains the low-pass. The echo crosses the tube three times, and the EQ only makes up for one crossing. Relative to the direct sound, the echo is therefore r_rx·r_src·|H_w|². The defaults are r_rx = 0.1 and r_src = 0.9 *(model)*. r_rx was lowered from 0.3 so the LF ripple stays under the ~±0.7 dB seen on the factory chart (doc 06 §5).

| | 100 Hz | 1 kHz | 10 kHz |
|---|---|---|---|
| Echo relative level, A | −28 dB | −43 dB | < −80 dB |
| Echo relative level, B | −27 dB | −41 dB | < −80 dB |
| Peak-to-peak ripple, A (by band) | 0.9 dB (40–200) | 0.5 dB (200–1k) | 0.1 dB (1–3k), ≈0 (3–10k) |

(Current defaults: 12.7 mm bore, Cooper loss law. With the old 14.3 mm/Kirchhoff tube the echo was −25 / −33 dB and the LF ripple 1.1 dB.)

**Ripple spacing = 1/(2τ):** **31.25 Hz on A and 35.7 Hz on B.** On a log axis the ripple looks dense at HF and sparse at LF, which matches UA's Figure 1. **The exact reflection coefficients are still unmeasured.** The factory chart only bounds them, so treat them as calibration parameters.

The quarter-wave stub (≈ 66 Hz fundamental) and the 30 Hz / 50–70 Hz end tuning concentrate their cancellation at low frequencies. That is where the echo is strongest, so the design effort went where the problem was.

## 6. Cascade mode (A → B, 30 ms)

- The main arrival is at 30 ms. The two echoes arrive at **58 ms and 62 ms**, i.e. τ_A+τ_B plus 2τ_B (B's own echo) and plus 2τ_A (A's echo), **4 ms apart**.
- When the two echoes are equal in size and filtering, they cancel completely at certain frequencies. The first null is at 1/(2·4 ms) = **125 Hz**, repeating every 250 Hz. The fast ripple therefore swells and collapses with a period of 250 Hz (Berners' "2-coil" response).
- The cascade also stacks both lines' coloration: two transducer pairs, two pre/post EQs, and both tubes' losses. The UA plug-in's "Coils" switch (single vs double) preserves exactly this difference at any delay time.

## 7. Transducer and cavity coloration: the "slow wiggles"

Berners attributes the broad curvature and the band-edge rolloffs to the transducers and their cavities. Contributors:

- Two dynamic cartridges in series, one run as a driver and one as a pickup. Each adds its own resonances, LF rolloff, and HF rolloff.
- Front-cavity resonance. Cooper moved one from ~9 kHz to ~1 kHz with a flare cap.
- Helmholtz slugs that leave small residual peaks and dips, adjusted by ear and meter on each unit.
- The 3–4 kHz compensator region.

This is UA's "Color A" (original emphasis). "Color B" is a leveled version.

## 8. Nonlinearity

Three possible sources, from most to least significant at normal levels:

1. **Acoustic waveform steepening in the tube.** Wave crests travel faster than troughs, c₊ = c₀ + ½(1+γ)v. Cooper's estimate is **D ≈ ½(1+γ)·v·τ·(2π/λ) × 100%**. Distortion grows with particle velocity, with path length, **and with frequency**. At his design intensity (v ≈ 6.5 mm/s) it is about **1% at 5 kHz** (model reproduces 1.007%), and about 10% at 10× that velocity. HF pre-emphasis raises HF velocity, which explains the brochure's warning about measuring at full output. It is mostly 2nd-order: 2nd harmonic plus difference/sum intermodulation.
2. **Driver excursion limits.** Cooper's R-50 started buzzing around 10 V drive near 200 Hz. The UREI factory procedure tests for the R50 diaphragm striking its resonator cover plate at 100 Hz, +10 dBm out. This is a hard, one-sided LF limit that only shows up when the input is driven hard. The send EQ's bass cut normally keeps it out of reach.
3. **1109 amps and transformers.** Class-AB op-amp with transformer I/O. Expect gentle LF transformer saturation and low-order harmonics. The THD spec is <0.5% at +4 dBm, so the effect is subtle at nominal level.

## 9. Noise and crosstalk

- **S/N > 70 dB** (15.7 kHz BW). Noise enters mainly at the R70 pickup and the receive 1109, so after the tube, and is then shaped by the receive EQ. That EQ adds a **+13 dB resonant lift at 10.4 kHz** and a modest LF lift. So expect hiss concentrated in a band around 8–11 kHz with a steep cutoff above, plus a little low-end rumble. It is not white noise. Acoustic noise picked up through the isolation is low thanks to the springs and Flo-Pak packing.
- **Separation > 40 dB.** The production test is tighter and more telling: **< −45 dB at 1 kHz but only < −25 dB at 10 kHz**. Leakage climbs steeply with frequency, which fits capacitive coupling between the two channels' high-level, pre-emphasized send signals in the shared cables and housing.
- **External acoustic leakage:** 80 dB SPL at the housing gives < −60 dBm out, which is negligible.

## 10. Psychoacoustics: why 14–16 ms works

- Cooper used a Haas/precedence range of about 2–20 ms. Within it, a delayed copy adds space and loudness without being heard as an echo. The 1972 *Audio* column puts the echo threshold at about 35 ms, with some estimates up to 50 ms.
- Different delays on the two channels give different comb patterns L and R. Panned A-left and B-right, that decorrelation is what produces the width.
- Summed with the dry signal, the delayed copy makes comb notches at f = (2k+1)/(2τ). On A the first notch is at 31.25 Hz with **62.5 Hz spacing**. On B it is 35.7 Hz with **71.4 Hz spacing**. (Don't confuse this with the internal echo ripple in section 5, which has half that spacing.) The tube's HF rolloff makes those notches shallower at HF, so the doubling sounds less "phasey" than a clean digital delay at the same time. That is one of the device's key traits.
