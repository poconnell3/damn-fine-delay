# 07: Field Research: Tube Size, Capsules, Measurements, Folklore

Follow-up research on the open questions in `00-index.md`. Sources: R/E/P and Gearspace repair and DIY threads, Shure catalogs and spec sheets, UA's internal photos, and a (now defunct) commercial IR pack listing. Each finding is labeled **Confirmed** (documented), **Estimated** (derived by us), or **Folklore** (unsupported recollection).

## 1. Tube inner diameter: Estimated ≈ 12.7 mm (½")

No document gives the production bore, so we measured it from UA's photo of an opened unit (`blog_cooper_time_cube_power_feat5`).

**Scale reference.** The coils are tie-wrapped to a pegboard panel. Standard pegboard has 1″ hole pitch. In the photo the holes form a 45°-rotated square grid: row spacing ≈ half the in-row spacing, which gives about 136–141 px per inch at 4× crop. Two checks on that scale:

- The aluminum driver housing measures about 1.35–1.4″ OD. It holds a 1⅛″ R50 cartridge, so that fits.
- The copper receiver riser measures about 0.95″ OD. It holds the ¾″ R70, which fits ⅞″ copper.

**Measurement.** The white delay tube is about 0.53–0.62″ OD at three points measured perpendicular to the tube. The tube sits in front of the pegboard, so perspective makes it look slightly larger, not smaller. The most likely stock size is **⅝″ OD × ½″ ID** polyethylene: ID ≈ 12.7 mm, plausible range 11–14 mm.

**Other evidence.**

- The green stub hose is slightly larger, about 0.75–0.8″ OD.
- Terry Manning (R/E/P) jokingly asked whether anyone had modded a unit "up to 7/8" hose." That implies the stock tube is smaller than ⅞″.
- An early unit, serial "Demo 99," used food-grade hose on pegboard with handwritten notes. That matches the photo.

**Consistency check (doc 06 §2.3).** With a ½″ bore and Cooper's measured loss law (1.6× Kirchhoff), the schematic EQ × tube implies an R50 + R70 pair that is:

- within about **±2 dB from 200 Hz to 7 kHz** (+1.9 dB at 200 Hz, −1.0 at 2 kHz, −2.3 at 7 kHz)
- about −5 dB at 31 Hz
- about −10.6 dB at 10 kHz

That is a physically sensible pair of 1970 dynamic cartridges. The old 14.3 mm/Kirchhoff assumption required −21 dB at 10 kHz.

**Model change.** `ctc_reference_model.py` now defaults to `diameter = 12.7 mm` and `loss_scale = 1.6`. Bore remains a parameter.

## 2. The capsules: Confirmed

**Driver, R50.** Per the Shure cross-reference AL377B and the 1966 catalog, the R50 is the replacement cartridge for these Shure models:

- **540/540S Sonodyne II**
- **560 lavalier**
- **561 omni** (40 Hz–10 kHz, 200 Ω actual)
- **575S/575SB**

It is an **omnidirectional** dynamic element. The 561 data sheet curve (digitized by eye, re 1 kHz) is:

| 40 Hz | 100 | 200 | 400 | 1k | 2k | 3k | **4.2k** | 5.5k | **7k** | 8.5k | 10k |
|---|---|---|---|---|---|---|---|---|---|---|---|
| −17 | −10 | −5 | −0.5 | 0 | +1 | +3.5 | **+6** | +2 | **−4.5** | −1 | −2 |

The **notch near 7 kHz is the same "icicle"** the UREI calibration procedure fights. It is a property of the R50 itself, which is why UREI ordered a special diaphragm (99H350B). The bass rolloff in this curve partly reflects the 561's speech voicing and free-field use. Inside a sealed tube coupler the low end behaves differently, so use only the HF features as model seeds.

**Pickup, R70.** The R70 is the cartridge of the **Shure 570/570S lavalier**: "50–12,000 cps, shaped … with rising response to 6,000 cps," 150 Ω. A Gearspace owner, Steve McDonald, measured the pickup capsule at **¾″ diameter × 1⅛″ long**, matching Cooper's description of the R70.

**Field replacements.** John Noll's 2008 R/E/P repair notes say standard **SM57 capsules** fit and were used as replacements. His unit's original was "the smaller silver version" with a metal resonator cover and a small **rubber O-ring glued on to flatten the ~7 kHz region**. His observations:

- The driver measured about 200 Ω.
- Moving that O-ring onto a modern SM57's plastic resonator cost high end, so he left it off.

So many surviving units now run SM57 cartridges **without retuning the Tee-block screws**. That is a likely source of unit-to-unit differences heard today, and it supports a "Re-capped (SM57)" unit variant.

## 3. The "400 Hz–4/5 kHz bandwidth" claim: Folklore, now traced

- It traces to Fletcher (R/E/P, 2006–2009) and was repeated by Kiwi on Gearspace. Fletcher described the response as "like 400Hz-4kHz." He credited part of it to the 1109 amplifiers, which he recalled as "limited bandwidth, … like 500Hz to 5kHz."
- **The documents contradict it.** The 1109 data sheet gives ±0.5 dB from 30 Hz to 20 kHz. The factory chart for serial 117 is about ±0.7 dB from 20 Hz to 11 kHz.
- The same threads also misstate the tube lengths: "12 and 14 ft," against the documented 15.82 and 18.08 ft.
- **Likely explanations:**
  - aging capsules
  - SM57 re-caps without Tee-block retuning
  - unmaintained stub/air-leak seals (the manual blames 200 Hz and 400 Hz dips on air leaks)
  - and above all, perception: a steep cutoff above 11 kHz plus a delayed copy placed −3 dB under the dry signal reads as "midrange-y"
- **Decision unchanged.** Authentic means the factory curve. A **"Worn" unit setting** may narrow the band, but not to 400 Hz–5 kHz by default.

## 4. Lineage

**Confirmed (oral history).** Brian Kehew interviewed **Ray Lubow** (Tel-Ray, Morley). Lubow built a 1950s "fake stereo" hose-delay box of about 25–27 ms using **copper** tube. He considered copper the most musical and disliked UREI's plastic hose. A surviving product of this kind is the Radio Craftsmen **"Xophonic"**, which has metallic tubing inside.

This is useful for an optional **"Copper Line" model**. Per Cooper, copper has lower and smoother loss than plastic, which showed "anomalous" loss at 1–5 kHz. One R/E/P poster suggests the copper line also adds more diffusion or smear.

## 5. Real-unit measurements: none public; the one commercial capture is gone

- No published impulse responses, THD measurements, or echo-level measurements of real hardware turned up in forums, papers, or manufacturer material. R/E/P users asked for IRs in 2006 and none were posted.
- **The one commercial IR pack is no longer available** (confirmed 2026-11-06). samples.but.happy listed "Cooper Time Cube Impulse Responses" (A, B and A→B cascade AIFs from an original unit), but the store is gone. Search engines still index the page text; nothing else survives. Wave Arts' Echo Space vintage-delay IR library does not include a Time Cube.
- **What we can still say about the echo without a capture.** UA's Dave Berners confirms a small reflection bump at 2× the delay after the main arrival, which fixes its position at 3τ. Its strength is bounded from above by the factory chart: a ±0.7 dB ripple allows an echo no stronger than about −22 dB at low frequencies. The model sits at −25 dB at 40 Hz and −28 dB at 100 Hz, so the real echo can be at most ~3 dB stronger. It may be weaker, in which case it is even less audible.
- Steve St. Croix reportedly wrote about trying to model the Cooper with early convolution software in *Mix*, in the late 1990s, according to a 2006 R/E/P recollection. We have not located the article.
- No public data exists for THD versus level and frequency. It needs a bench measurement.

## 6. Updated defaults (reference model)

| Parameter | Old | New | Why |
|---|---|---|---|
| Bore | 14.3 mm | **12.7 mm** | Photo measurement (§1) |
| Loss law | Kirchhoff ×1.0 | **×1.6 (Cooper measured)** | Makes the EQ/tube/transducer budget self-consistent (§1) |
| r_rx | 0.1 | 0.1 | Unchanged |

Results with the new defaults:

- **Line A loss:** 3.6 dB at 100 Hz, 11.2 dB at 1 kHz, 35.6 dB at 10 kHz
- **Echo re direct:** −28 dB at 100 Hz, −43 dB at 1 kHz
- **LF ripple:** about 0.9 dB peak-to-peak, inside the factory chart
- **Raw-tube excess group delay:** about 325 µs at 100 Hz before EQ; the EQ largely cancels it

## 7. What is still open

1. **Exact bore.** Calipers on a real unit would settle it.
2. **Echo level and THD.** No purchasable data, and no physical unit is available to measure (decided 2026-11-06). The echo stays a tuning constant bounded by the factory chart; THD is calibrated to the brochure's <1% limit.
3. **R70 response curve.** No 570 curve was found, only the text spec.
