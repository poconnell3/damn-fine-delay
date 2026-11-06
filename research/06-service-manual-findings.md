# 06: Service Manual Findings (UREI 920-16 Instruction Manual + Production Calibration Procedure)

Source: the 33-page scanned "Model 920-16 Cooper Time Cube Instruction Manual" supplied by Patrick. It contains:

- the brochure and specs
- Sections I–V (description, theory of operation, installation, operation, maintenance)
- a factory frequency-response chart
- schematic **D11688** (dated 11-21-71)
- the 1109 amplifier data sheet and schematic **B11195C**
- the internal **Production Calibration Procedure**
- a handwritten "14 MS Line Specimen #2972" assembly sketch

This doc settles open questions 1, 3, and 4 from `00-index.md`, fully or in part. The companion script `tools/ctc_eq_networks.py` computes the EQ responses below from the schematic values.

## 1. Facts confirmed or added

| Item | Manual says | Effect on the model |
|---|---|---|
| Speed of sound | 1,130 ft/s; ".884 ms per foot" | Same as the patent |
| Line lengths | Sketch: 15.82 ft ≈ 15′9¾″ = 14 ms; 18.08 ft ≈ 18′1″ = 16 ms | Confirmed |
| Transducers | Driver **Shure R50**, receiver **Shure R70**. UREI used a special R50 diaphragm, part **99H350B**. The standard 99A350B had a 3–5 dB notch near 7 kHz. | The transducer model should carry a residual ~7 kHz notch and a slight ~9 kHz dip |
| Pre-EQ | "High frequency pre-emphasis and midrange equalization," **factory-adjusted to match the individual drive transducer**. It sits in the feedback loop of the SEND 1109. | Transcribed in §2 |
| Post-EQ | A feedback equalizer giving "factory-adjusted calibration at low and high frequencies," in the RECEIVE 1109 | Transcribed in §2 |
| Housing | Lines suspended on "special springs"; housing filled with **Flo-Pak styrofoam pellets** | Confirmed |
| Acoustic isolation | 80 dB SPL at the enclosure gives < −60 dBm at the outputs (normal level +4 dBm) | External acoustic leakage is ≥ 64 dB below program. Ignore it. |
| Crosstalk (production test) | Feed B, read A: **< −45 dB at 1 kHz, < −25 dB at 10 kHz** | Crosstalk **rises steeply with frequency**. Model it as a high-pass-shaped leak, not a flat −40 dB. |
| HF stress | A 1/8 A fuse protects the driver "from damage due to excessive high frequency levels." Sustained levels well above 0 VU, or tape rewinding against the heads, can blow it. | The HF pre-emphasis really does push the driver hard. Supports an HF-sensitive driver/air nonlinearity. |
| Measuring | Measure frequency response at **−10 VU (−6 dBm)** because of the pre-emphasis. Factory calibration ran at −5 dBm. | Use the same levels when we calibrate against data |
| LF limit | Calibration step 8: at 100 Hz and +10 dBm output, a bad waveform means the **R50 diaphragm is striking the resonator cover plate** | This is the driver's hard LF limit: an asymmetric, hard-ish clip starting around +10 dBm at 100 Hz on a marginal unit. Good units are clean there. |
| Usage tip | For doubling, mix the delayed signal about **−3 dB** under the direct, to avoid comb-filter coloration | Make this the default doubling preset level |
| Why 14 vs 16 ms | Different delays "minimize comb filter effects" when the quad channels are recombined to mono | Confirmed |
| Cascade | 30 ms approximates a first-order reflection from about 40 ft | For the reverb pre-delay preset |
| Haas range | Fusion from 2 ms to about 35 ms | Confirmed |

## 2. The equalizers (schematic D11688)

Both EQs use the 1109 as a non-inverting amplifier. The network sits between **pin 9 (EQ FEED, output side)**, **pin 10 (EQ RETURN, inverting input)**, and **pin 8 (EQ GND)**. Channel A and Channel B use identical values: A uses R1–R7, C3–C7, L1, R16–R23, C13–C17, L3; B uses R8–R14, C8–C12, L2, R24–R31, C18–C22, L4.

### 2.1 SEND (pre-emphasis), gain = 1 + Z_f / Z_g

- **Z_f (pin 9 → 10):** R2 3.9 kΩ ‖ C7 1 nF
- **Z_g (pin 10 → 8)** is three branches in parallel:
  1. **L1 25 mH + C4 9.4 nF + R6 120 Ω** in series. Series resonance at **10.4 kHz** gives a peaked HF boost.
  2. **C5 68 nF + R7 120 Ω** in series. A rising HF shelf, about +6 dB/oct above ~600 Hz.
  3. **R3 1.8 kΩ + R4 5 kΩ trimmer "MID RANGE" + (R5 10 kΩ ‖ C6 0.15 µF)**. A low/mid shelf; R4 sets the midrange tilt. The calibration procedure sets it near mid-range at 700 Hz.
- **Output network:** R1 120 Ω in series, C3 0.1 µF in shunt, into the 200 Ω R50. About −1 dB at 10 kHz and −3 dB at 20 kHz.

Computed response, dB re 1 kHz (MID RANGE = 2.5 kΩ):

| 32 Hz | 100 | 200 | 500 | 2k | 3k | 5k | 7k | **10k** | 12k | 20k |
|---|---|---|---|---|---|---|---|---|---|---|
| −7.0 | −6.4 | −5.2 | −2.6 | +4.0 | +6.9 | +11.0 | +14.1 | **+22.3** | +13.8 | +17.5 |

So the send stage **cuts bass by about 6–9 dB** (protecting driver excursion) and **boosts treble steeply**, with a resonant peak at 10.4 kHz.

### 2.2 RECEIVE (post-EQ): a T-network feedback with a series-resonant shunt

- **Series arm:** pin 9 → R16 560 Ω → node Z → (R18 47 kΩ ‖ C16 0.1 µF) + (R17 18 kΩ ‖ C15 470 pF) → pin 10
- **Shunt from Z to ground:** **L3 25 mH + C17 9.4 nF + R23 500 Ω trimmer "10KC"**. Resonance at **10.4 kHz**; R23 sets the depth of the 10 kHz boost.
- **Pin 10 → ground:**
  - C13 22 nF + R20 120 Ω (HF rise)
  - ‖ R19 1.5 kΩ + (C14 0.22 µF ‖ (R21 2 kΩ trimmer "L.F." + R22 560 Ω)). This is the LF boost, set by R21.

Computed response, dB re 1 kHz (L.F. = 1 kΩ, 10KC = 500 Ω):

| 32 Hz | 50 | 100 | 200 | 500 | 2k | 3k | 5k | 7k | **10k** | 12k | 20k |
|---|---|---|---|---|---|---|---|---|---|---|---|
| +3.1 | +1.3 | −1.6 | −3.0 | −2.0 | +1.7 | +2.7 | +4.7 | +7.1 | **+13.5** | +12.5 | +10.6 |

### 2.3 What the two EQs do together

- The combined send × receive EQ peaks at **10.3 kHz, about +36 dB re 1 kHz**. Most of that boost makes up for tube and transducer losses.
- **Treble split:** about 60% of the HF boost (in dB) is *pre*-emphasis. Bass is cut before the tube and restored after it, so LF noise from the pickup and receive amp gets a small boost.
- With the Cooper-law tube loss (14.3 mm bore) included, the electronics alone leave the response 0 to +14 dB high between 1 and 10 kHz. The factory chart is flat, so the **R50 + R70 pair and their cavities must supply the inverse**. That implied transducer-pair curve is:
  - about −4 dB at 31 Hz
  - +2.6 dB at 200 Hz
  - −2 dB at 3 kHz
  - −4 dB at 7 kHz
  - about **−13 dB at 10 kHz**

  It is a plausible curve for 1970 dynamic cartridges and is the best estimate we have until a unit is measured.

  **Update (doc 07):** with the photo-estimated 12.7 mm bore, the implied pair is flatter: about −5 dB at 31 Hz, −1 to −2 dB from 2 to 7 kHz, and **−10.6 dB at 10 kHz**. That bore is now the model default.
- **Top-end cutoff:** above about 10.5 kHz the EQ resonance is spent, while tube loss and transducer rolloff keep rising. The result is a **steep cutoff just above ~11 kHz**. Both channels in the factory chart (serial 117) fall almost vertically at about 11–12 kHz. That may be partly where the sweep ended, but it fits the circuit. **This "brick-wall" top is a defining trait** and one reason the unit sits back in a mix.

### 2.4 The three per-unit trimmers become plugin parameters

| Trimmer | Range | What it does | Factory practice |
|---|---|---|---|
| R4 "MID RANGE" (send) | 0–5 kΩ | Midrange tilt of the pre-emphasis (~700 Hz) | Start mid-range |
| R21 "L.F." (receive) | 0–2 kΩ | LF boost below ~100 Hz | Start mid-range at 100 Hz; final touch-up of 30–100 Hz. Using it at either extreme means "something else is wrong." |
| R23 "10KC" (receive) | 0–500 Ω | Depth of the 10 kHz resonant boost | Start fully CCW |

These set the **Unit** and **Tuning** parameters with real component values instead of invented randomness.

## 3. The 1109 amplifier (schematic B11195C, dated 4-27-70)

- **Circuit:** discrete. 2N5089 differential input pair (Q1/Q2, selected for low noise), 2N5087 stages (Q3/Q4), and a complementary output pair, 2N3053/2N4037 (Q5/Q6, matched), biased by 1N4003 diodes. That makes it a class-AB output stage. Input transformer T1 (B11178), output transformer T2 (B11148), and a 400 µF output coupling cap.
- **Data sheet:**
  - ±0.5 dB from 30 Hz to 20 kHz
  - +30 dBm maximum output on a 30 V supply
  - THD < 0.5% from 30 Hz to 20 kHz at +24 dBm; typically < 0.05% midband
  - EIN −126 dBv (600 Ω source, 15.7 kHz bandwidth)
- **Gain strapping:** Rx from pin 9 to 10 with 1.8 kΩ from pin 10 to 8. 1.5k / 9.1k / 36k give 30 / 40 / 50 dB. This confirms the gain = K·(1 + Z_f/Z_g) form used above, with K ≈ 24.7 dB contributed by the transformers.
- **Modeling takeaway:** at Time Cube levels (+4 dBm nominal, far below +24) the amps are very clean. Their audible share is **transformer LF behavior and the EQ curves**, not amp distortion. Keep the 1109 saturator subtle. It matters mainly when Drive is pushed or inside the extended feedback loop.
- **Supply:** the Time Cube runs the 1109s from a single +30 V regulated rail (Zener plus MJE3055 pass transistor).

## 4. Acoustic tuning knowledge (Production Calibration Procedure and line sketch)

The calibration procedure gives the **known failure and variation modes** of each line. That is exactly what we need for an honest "unit variation" model.

**Order of calibration.**

1. Set 1 kHz to −5 dBm.
2. Rough-set the **3 kHz compensator** (it removes a **3 kHz peak**).
3. Get 100 Hz–1 kHz within ±1¼ dB.
4. Check the R50 for LF clearance.
5. Tune 30–100 Hz, then refine the compensator.
6. Fix 1–10 kHz with the screws.
7. Target ±1 dB (the spec is ±2 dB).

**Where anomalies come from.**

- **Below 100 Hz:** the stub and air leaks.
- **100 Hz–1 kHz:** driver housing damping and air leaks.
  - An air leak at the receiver cap gives a **~200 Hz dip**.
  - An air leak at a housing screw gives a **~400 Hz dip**.
  - Felt wrapped too tightly gives a mid-HF rise at 1–3 kHz, an LF rolloff, and an irregularity around 700 Hz.
  - A thick O-ring or poor seal gives a low-end rolloff.
- **1–10 kHz:**
  - The **"icicle"**, a sharp notch at **6.7–7.2 kHz**. It is 3–5 dB with the standard R50 diaphragm and 0.5–1 dB with UREI's special one. It is reduced by pressing the R70 receiver housing further into the Tee. Too much pressure gives a **peak at 8–8.5 kHz**.
  - The special diaphragm also has a slight **~9 kHz** hole.

**The screws (Helmholtz slugs in the Tee block).**

- **Dip screws D1 and D2 (6-32):** dip filters for 4.5–7 kHz. D1 takes out the lowest-frequency peak and D2 the highest. Example: peaks at 5 kHz and 8.5 kHz → D1 handles 5 kHz, D2 handles 8.5 kHz.
- **Peaking screws P3–P7:** fill in holes.
  - A ¼" screw handles 6–8 kHz with a slight peak.
  - 4-40 screws fill 8.5–11 kHz holes.
  - P3 has the largest diameter and the strongest peaking effect.
  - P6 and P7 (the back screws) are used last.
- There is "considerable interaction" between the dip and peak screws.

**Stub details.**

- A cork end plug with a **3/16" hole**.
- Foam damping 3/4"–1" from the end.
- A **2′9″** (33″) vent hole under the damping clamp. That is closer to the patent claim's 32″ than to the 39″ in its description.
- Poly-cotton fill between the 3 kHz compensator and the foam.
- The compensator sits about ¼" from the block end of the nipple. Adjusting its position takes out the 3 kHz peak.

**Unit variation model.** Each simulated "unit" is drawn from these documented residuals within the ±1 dB / ±2 dB tolerance:

- a 6.7–7.2 kHz notch of 0.5–3 dB
- an optional 8–8.5 kHz peak of 0–1.5 dB
- a ~9 kHz dip of 0–1 dB
- a residual 3 kHz bump of ±0.7 dB
- 200 Hz / 400 Hz dips of 0–1 dB (air leaks)
- an LF wobble below 100 Hz (stub)
- the three electronic trimmer positions (§2.4)

## 5. Factory frequency response (serial 117, A and B)

- B&K level recorder; pen trace from 20 Hz to about 11.5 kHz at 1 dB per division, 0 dB reference = −5 dBm.
- Both channels are within roughly **±0.7 dB** over that range. Small bumps near 70 Hz and 200 Hz, small dips near 35 Hz and 4–5 kHz. Steep edges at 20 Hz (the recorder's lower limit) and at about 11.5 kHz.
- **Effect on the echo model:** the low-frequency ripple from the 3τ echo must be ≲ 1 dB peak-to-peak. The reference model's default reflection was therefore lowered from r_rx = 0.3 to **0.1**. That gives about 1.1 dB p-p at 40–200 Hz and an echo at about −25 dB re the direct sound at 100 Hz and −33 dB at 1 kHz. The recorder's 25 mm/s pen speed smooths fast ripple, so this is an upper bound on what we can see, not a precise measurement. *(Those figures assume the earlier 14.3 mm tube. At the doc 07 defaults, 12.7 mm with Cooper loss, they become 0.9 dB, −28 dB, and −43 dB.)*

## 6. What's still unknown

- The production tube's inner diameter. The manual and sketch give lengths only. *Doc 07 estimates about 12.7 mm from photos.*
- The absolute R50/R70 responses. We have the implied pair curve (§2.3), plus the R50's published Shure 561 curve (doc 07 §2).
- How far the top-end cliff at ~11.5 kHz is real versus the end of the sweep.
- THD versus frequency at high level (the manual only warns about it).
