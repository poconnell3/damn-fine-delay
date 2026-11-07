# Red Room Delay: Research Index

Research base for **Red Room Delay** by Firewalk Audio, a VST/AU plugin that emulates the UREI / Universal Audio Model 920-16 Cooper Time Cube (1971), a dual acoustic delay line that sends audio down coiled plastic tubing. Name decided 2026-11-06; see doc 04 §5.

## Documents

| Doc | What's in it |
|---|---|
| `01-hardware-reference.md` | History, factory specs, signal path, transducers, tubing, the patented termination/tuning system, electronics, controls. Corrections to common claims. |
| `02-acoustics-and-physics.md` | Why it sounds the way it does: tube losses, dispersion, reflections and the "wiggly" response, cascade behavior, nonlinear propagation, noise, temperature. With worked numbers. |
| `03-dsp-emulation-design.md` | How to model each stage digitally, recommended architecture, nonlinearity, oversampling, calibration and test plan. |
| `04-existing-emulations-and-feature-plan.md` | UA Cooper Time Cube Mk II and AudioThing Echo Cube feature sets; proposed control set for our plugin. |
| `05-sources.md` | Annotated bibliography with links. |
| `06-service-manual-findings.md` | From the full UREI service manual: transcribed send/receive EQ networks, 1109 amp details, factory frequency response, the production calibration procedure, and the per-unit tuning variations. |
| `07-field-research-findings.md` | Tube bore estimated from photos (≈12.7 mm), R50/R70 capsule identities and curves, where the "400 Hz–5 kHz" folklore came from, the Ray Lubow/Tel-Ray copper-tube lineage, and why no real-unit IRs are available. |
| `tools/ctc_reference_model.py` | Offline numpy reference model of the acoustic path. It reproduces Cooper's published loss figures and the patent's tube lengths. Use it as the golden reference for C++ tests. |
| `tools/ctc_eq_networks.py` | Send and receive EQ frequency responses computed from schematic D11688 component values. |
| `../plugin/` | Real-time C++ engine and JUCE plug-in, v0.1 (VST3, CLAP, Standalone; AU on macOS). 95 C++ checks, pluginval strictness 10, null-tested against `prototype/`. See doc 03 §9. |
| `prototype/` | Python golden-reference engine (full signal path, both lines, routings), 19 spec tests, demo renderer. README lists the chain, calibration choices and measured results. Listening page: https://claude.ai/artifact/Gtqp6FdMS5FuKjbpDXzKE3 |

## Key facts at a glance

| Item | Value | Source |
|---|---|---|
| Model | UREI 920-16 "Dual Acoustical Delay Line" | Factory brochure |
| Designers | Dr. Duane H. Cooper (Univ. of Illinois), M. T. "Bill" Putnam (UREI) | Brochure, US 3,789,328 |
| Delays | Ch A 16 ms, Ch B 14 ms, cascaded 30 ms | Brochure |
| Tube length | 14 ms = 15.82 ft, 16 ms = 18.08 ft (at 1,130 ft/s) | US 3,789,328 |
| Tube | Coiled white polyethylene, estimated ⅝″ OD × ½″ ID (≈12.7 mm) from photos; green ¼-wave stub hose. Cooper's prototype was copper, ID ~9/16″. | Doc 07; Audio Jul 1972; Audio Apr 1971 |
| Transducers | Shure **R50** driver (UREI special diaphragm 99H350B) and Shure **R70** receiver. Later service units reportedly use SM57-type capsules. | Service manual; UA blog |
| Frequency response | ±2 dB, 40 Hz–10 kHz (typ. ±1.5 dB). The factory chart for serial 117 is about ±0.7 dB from 20 Hz to ~11 kHz, then a steep cutoff | Brochure; service manual |
| Equalization | Send: bass cut plus a steep treble pre-emphasis peaking at 10.4 kHz. Receive: LF boost plus a 10.4 kHz resonant boost. Combined about +36 dB at 10.3 kHz re 1 kHz. | Schematic D11688 (see doc 06) |
| Crosstalk | < −45 dB at 1 kHz, < −25 dB at 10 kHz | Production calibration procedure |
| THD | <1% (typ. <0.5%) up to +4 dBm | Brochure |
| S/N | >70 dB (15.7 kHz bandwidth) | Brochure |
| Channel separation | >40 dB | Brochure |
| Wow & flutter | 0% | Brochure |
| Electronics | 4× UA 1109 card amps (transformer in/out), pre- and post-EQ networks, HF pre-emphasis | Brochure; Audio Jul 1972; GroupDIY |
| Housing | 24" × 24" × 9" plywood, coils on springs, packed with foam pellets | Brochure; UA blog |
| Units made | ~1,000 | UA |
| 1972 price | $850 | Audio Jul 1972 |

## Corrections to the brief

- **Bandwidth.** The brief says "roughly 400 Hz to 5 kHz bandpass." That figure traces to forum recollections (doc 07 §3), which also misremember the 1109 amps as band-limited and the tube lengths. The factory spec is ±2 dB from 40 Hz to 10 kHz. That is wide, and it is flat only because of heavy equalization. A narrow, honky band fits plastic garden hose without proper EQ, which is what DIY builds and AudioThing's "Garden Hose" model reproduce, and possibly worn units. **Decision:** the authentic model follows the factory spec. The narrow band becomes an optional "Hose/Worn" model.
- **Transducers.** The brief calls the driver "a small speaker driver." It was a Shure dynamic *microphone* cartridge run in reverse as a speaker, with a second cartridge as the pickup.
- **Delay times.** 14, 16, and 30 ms are correct. The two lines differ on purpose, so that using both at once does not produce matching comb filters.

## Open questions (blockers for "authentic" accuracy)

1. ~~Pre-EQ and post-EQ network curves.~~ **Resolved.** Transcribed from schematic D11688 in doc 06.
2. **Production tube inner diameter.** *Estimated.* About 12.7 mm (½″ ID), plausible range 11–14 mm, measured from photos against the 1″ pegboard pitch (doc 07 §1). This is now the model default, together with Cooper's measured loss law (×1.6). Calipers on a real unit would confirm it.
3. **Measured response of a real unit.** *Partly resolved.* The factory chart caps the ripple at about ±0.7 dB, and the production procedure documents the typical residual peaks and dips. No public IRs or THD data exist. The one commercial IR pack (samples.but.happy) is no longer sold, and no physical unit is available to measure (decided 2026-11-06). The model is calibrated to the factory documents and physics; the factory chart bounds the echo to at most ~3 dB above the model (doc 07 §5).
4. **Transducer frequency responses.** *Mostly resolved.* The R50 is Shure's omni dynamic element (561/560/575S). Its published curve shows a +6 dB peak at about 4 kHz and the **7 kHz notch** that UREI fought. The R70 is the 570 lavalier element, specified 50 Hz–12 kHz with a rise to 6 kHz. No R70 curve was found.
