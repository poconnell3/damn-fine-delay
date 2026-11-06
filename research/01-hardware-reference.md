# 01: Hardware Reference: UREI 920-16 Cooper Time Cube

## 1. History

- **1971, the prototype.** Dr. Duane H. Cooper (University of Illinois) published a two-part build article, "Construction of a Madsen-System Delay Tube," in *Audio* (April 1971, p.18; May 1971, p.48). It described a coiled copper delay tube for synthesizing quadraphonic sound from stereo. The "Madsen system" refers to the delayed-rear-channel ambience idea credited to Madsen of Bang & Olufsen.
- **The UREI product.** Bill Putnam (United Recording / UREI) evaluated Cooper's unit and developed it at UREI. The main changes were polyethylene tubing (lighter, smooth bore, no ringing) and better acoustic equalization for a flatter, wider response. It was sold as the Model 920-16 for about $850, and roughly 1,000 were built.
- **Patent.** US 3,789,328, "Acoustical Delay Line" (Putnam & Cooper), assigned to United Recording Electronics Industries. Filed 22 June 1972, issued 29 January 1974.
- **Marketed uses.** Quad synthesis from stereo, "loudness enhancement" (doubling without electrical summing), spatial enhancement, pre-delay into echo chambers and plates, and delaying audio to optical film recorders so the noise-reduction system would not clip the first transients.
- **Studio use.** Pre-delay into EMT plates, doubling vocals and guitars, and Haas widening. It is still used at Blackbird and Sunset Sound, and is credited on records such as "Tell Me Something Good" (Rufus) and "Low Rider" (War).

## 2. System layout

Two boxes are joined by two 25 ft XLR mic cables:

1. **Delay-line housing.** A grey plywood box, 24" × 24" × 9", 36 lb, with a top handle. It holds two coiled tubes, each with a driver and a pickup. The coils hang on springs for acoustic isolation, and the box is packed with foam pellets ("packing peanuts") that also damp the coils.
2. **Electronics chassis.** 19" rack, 3.5" high, 7" deep, 12 lb. It holds the power supply, four UA 1109 card amplifiers (one input amp and one output amp per channel), the equalizing networks, a VU meter, and the controls.

### Signal path (per channel)

```
Line in (600 Ω, transformer-balanced)
  → 1109 input amp [Input Gain pot]
  → pre-EQ network (HF pre-emphasis + LF shaping)
  → XLR cable → DRIVER (Shure dynamic cartridge used as a speaker), foam back-loaded
  → tapered coupler → coiled rigid polyethylene tube (15.82 ft / 18.08 ft)
  → T-block cavity: PICKUP cartridge at 90° near the cavity center
        ├─ Helmholtz tuning slugs (screws) in the cavity walls
        └─ 3 kHz compensator → quarter-wave terminating stub (51 in, graded absorber, cork plug, side vent)
  → XLR cable → post-EQ network → 1109 output amp
  → Line out (600 Ω load, transformer-balanced)
```

Both of the 1972 sources confirm the pre- and post-equalization. The brochure warns that the HF pre-emphasis means distortion should not be measured at full output. In other words, loud HF content drives the transducer and the air column hard.

## 3. Factory specifications (brochure)

| Parameter | Spec |
|---|---|
| Delays | A = 16 ms, B = 14 ms; cascade = 30 ms |
| Frequency response | ±2 dB, 40 Hz–10 kHz (typically ±1.5 dB) |
| THD | <1% (typically <0.5%) at all levels up to +4 dBm out; does not rise at low levels |
| S/N | >70 dB (15.7 kHz noise bandwidth) |
| Channel separation | >40 dB |
| Wow & flutter | 0% |
| Input sensitivity | −20 to +20 dBm for +4 dBm out |
| Input / output | 600 Ω, transformer-isolated, floating; TRS jacks normalled to rear barrier strips |
| Controls | Input Gain ×2, Output Meter Transfer switch, Power |

Ad copy from 1972 also claims "bandwidth 30 Hz to 10 kHz," and says it costs less than a third as much as a single-channel digital unit.

## 4. Transducers

- **Patent:** a Shure R-50 cartridge as the driver, and a Shure cartridge as the receiver.
- **Cooper's 1971 build:** a Shure **R-50** driver (~1-1/8" diameter, 200 Ω, nearly resistive at 1 kHz, can take a few hundred mW, needs ~100 µW acoustic out). The pickup was a Shure **R-70** (~3/4", 200 Ω) behind a 10:1 step-up transformer (Shure 51A34).
- **Service manual (production):** driver **Shure R50**, receiver **Shure R70** (installed in an "R70 housing"). UREI ordered a special R50 diaphragm, **99H350B**, because the standard 99A350B had a 3–5 dB notch near 7 kHz. See doc 06.
- **UA (later units / service):** "Shure mic capsules (of the SM57 variety)" at both ends. These are probably replacements, since the R50 and R70 are long discontinued.
- **Mounting details that matter for tone:**
  - Cooper used a flare cap on the driver front cavity. It moved a cavity resonance from about 9 kHz down to about 1 kHz and lowered its Q.
  - A damping collar on the driver gave more than 5 dB of boost at a 2 kHz peak.
  - The pickup sits in a very shallow cavity (Cooper: a 0.22" deep tee).
- **Implication:** the response is the product of two dynamic cartridges in series, one run backwards, plus their cavities. The "slow wiggles" in UA's analysis come from these transducer/cavity resonances.

## 5. Tubing

- **Length:** delay = length / speed of sound. The patent uses 1,130 ft/s (344.4 m/s), giving 14 ms = 15.82 ft and 16 ms = 18.08 ft.
- **Material:** rigid-walled polyethylene in production. Cooper's prototype used soft copper (1/2" nominal: OD ~5/8", ID ~9/16" ≈ 14.3 mm), supplied in 50 ft coils with about a 1 ft coil radius. Cooper found that plastic tubing gave severe anomalous attenuation between 1 and 5 kHz. That is the likely reason for the patent's dedicated 3–4 kHz compensator, and for why "garden hose" DIY builds sound narrow and honky.
- **Inner diameter (production):** not published. The patent says only that it matches the housing's minimum opening. **Assumption for modeling: 14.3 mm, adjustable.**
- **Multimode limit:** Cooper's condition 0.6λ > d keeps the tube single-mode (plane wave) up to about 14 kHz for a 14.3 mm bore.

## 6. Termination and tuning system (US 3,789,328)

The patent solves one problem: a tube that is simply terminated reflects almost all the sound back. The reflections and standing waves make the response uneven. The patent adds the following parts:

| Element | What it does | Tuning range |
|---|---|---|
| **T-block cavity** (tooled aluminum) | Input tube enters one side and the terminating stub leaves the opposite side. The pickup is mounted at 90°, near the center of the cavity. | n/a |
| **Quarter-wave terminating stub** | 4 ft 3 in (51 in) long, with graded absorbent (least dense near the cavity, densest at the far end). Its returning wave arrives about 180° out of phase with the main-tube reflection, which partly cancels it. | Fundamental ≈ 66 Hz (1130 / (4 × 4.25 ft)) |
| **Cork end plug with central hole** | The hole diameter sets the response near 30 Hz. An iris or calibrated plugs were suggested as alternatives. | ~30 Hz |
| **Side vent on the stub** (¼" hole at 39"; the claim says 32") | Covered by absorbent under an adjustable clamp. Tightening it removes a peak near 70 Hz. | 50–70 Hz |
| **3 kHz compensator** (between the T-block and the stub) | Absorbent inside a beryllium-copper sheath. Lower frequencies pass through an open annulus. Screw 104 sets the frequency and screw 100 sets Q via absorbent density. | 3–4 kHz |
| **Helmholtz tuning slugs** (screws into holes in the cavity walls) | Slugs between the pickup and the incoming tube mostly act on the direct wave and tune **dips**. Slugs between the pickup and the stub mostly act on the reflected wave and tune **peaks**. Tip angle sets Q (flat, 45°, 60°, 80°; a 45° tip gives ~3 dB less reflection than a flat one). Slug diameter is varied to keep Q constant across frequency. | Example problems in the patent: a sharp spike at 8 kHz and a dip at 5.4 kHz |
| **Dense back-loading of both transducers** | Stops low-frequency anomalies from the transducers' rear radiation. | LF |

**What this means for the emulation:** every unit was hand-tuned with screws, so no two units sound identical. A "unit variation" or "tuning" parameter set is historically justified. The production procedure (doc 06 §4) names the tuning screws: two dip screws (D1/D2, 4.5–7 kHz) and peaking screws (P3–P7, 6–11 kHz). It also lists the typical residual faults: a 6.7–7.2 kHz "icicle" notch, an 8–8.5 kHz peak, and 200/400 Hz air-leak dips.

## 7. Electronics

- **UA 1109 card amp (schematic B11195C):** discrete, with a 2N5089 differential input, a class-AB complementary output (2N3053/2N4037), and input and output transformers. Rated ±0.5 dB from 30 Hz to 20 kHz, and below 0.5% THD at +24 dBm. It runs from a single +30 V rail and is very clean at the Time Cube's +4 dBm level.
- **Pre-EQ (SEND 1109 feedback loop):** cuts bass by about 6–9 dB and applies a steep treble pre-emphasis with a series-resonant LC peak at **10.4 kHz** (about +22 dB re 1 kHz). A "MID RANGE" trimmer (R4) is set per unit to match its driver.
- **Post-EQ (RECEIVE 1109 feedback loop):** an LF boost set by the "L.F." trimmer (R21), plus a second 10.4 kHz resonant boost whose depth is set by the "10KC" trimmer (R23).
- The combined EQ is about **+36 dB at 10.3 kHz re 1 kHz**, all spent making up for tube and transducer losses. Full netlists and curves are in doc 06.
- For comparison, Cooper's 1971 prototype used a simpler RC network: 40 µs and 10 µs time constants, about +16 dB at 12 kHz.
- **No compander or noise reduction** is described.

## 8. Controls and connections (original)

- Front panel: Input Gain A, Input Gain B, Meter Transfer switch, Power.
- I/O: two TRS inputs and two TRS outputs on the front, normalled to rear barrier strips. There is **no** dry/wet mix, feedback, or delay control on the hardware. Cascading is done by patching A's output into B's input.
- The output is **100% wet**. Doubling and Haas effects happen at the console, where the dry signal is mixed back in.

## 9. Character notes from users and makers

- It "sits in the mix," thickens, and widens. It is used for short doubling and pre-delay, not for audible echoes.
- AudioThing says preamps and gain staging add saturation and density before and after the acoustic path.
- Eventide's history piece says, wryly, that audio through it sounded like it had travelled down a garden hose. That remark is about the concept, not a measurement.
- UA reviewers often compare it to "tape-like" or "cement room" character, and to an Echorec.
