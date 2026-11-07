# 04: Existing Emulations and Our Feature Plan

## 1. Universal Audio Cooper Time Cube Mk II (UAD-2/Apollo; released 2009 with UAD v5.3)

UA says it is an "exacting emulation" and a "feature-enhanced improvement" on the original. It runs only on UAD-2 DSP. UA's Dave Berners documented the response modeling in "Wiggly Frequency Response and the Cooper Time Cube": transducer/cavity curvature plus an echo at 2τ after the main pulse, strongly low-passed. UA's Jonathan Abel co-authored the 2003 DAFx wall-loss filter paper we plan to use. UA has not said whether that paper was used for this plug-in.

**Controls (from UA manual):**

| Control | Scope | Range / behavior |
|---|---|---|
| Delay A / B | per channel | 5–2500 ms; Sync: 1/64 to 3/1 note values |
| Decay A / B | per channel | Feedback. Minimum is one repeat; maximum is "near infinite" |
| Pan A / B | per channel | Wet only. Disabled in mono→mono |
| Echo Volume A / B | per channel | Up to +10 dB, plus OFF |
| Coils | global | Single vs cascaded (double) coil sound, at any delay. Coil 1 allows longer Decay |
| Color | global | A = original filter emphasis, B = "leveled." B allows longer Decay. Subtle; depends on Coils and Decay |
| Treble / Bass | global | Cut/boost on the wet signal |
| HP Filter | global | 12 dB/oct, 20 Hz–12 kHz on wet; can be switched OFF |
| Gain | global | ±15 dB on wet+dry |
| Send | global | Mutes the delay input while trails continue |
| Wet Solo | global | 100% wet; saved with the session, not presets |
| Sync | global | Tempo-locks both delays |
| I/O | | MIMO / MISO / SISO (true stereo) |

**Takeaways:**

- Coloration is **decoupled from delay time**.
- The color choice changes the safe feedback range. That implies loop-gain limiting that depends on the color's peak gain, which matches our plan in doc 03 §4.

## 2. AudioThing Echo Cube (native; v1.0, March 2026)

- Based on an original, serviced unit in AudioThing's collection. They model the full path from capsule to tube to output stage, including preamp coloration. They don't say which DSP method they use.
- Two lines, A and B, each with Time, Feedback, and Level.
- **3 models:**
  - Time Cube (faithful)
  - Garden Hose (from their own experiments: tighter, more focused)
  - Corrugated (narrower bandwidth, rougher texture)
- **5 routings:** Serial, Parallel, Ping Pong, Mono Sum, Mid/Side.
- **Other controls:** Brightness, Contrast, randomizer.
- Users mention modulation and extended delay.
- **Formats:** VST2/3, AU, AAX, CLAP; macOS/Windows/Linux; iOS AUv3.
- **Price:** $59 (intro $29).

## 3. How we can stand out

1. **Physics-first parameters that make sense.** Coil length, bore, temperature, termination quality ("Tuning"), and a unit-variation seed. Neither competitor exposes the physical model this directly.
2. **Authentic mode with a strict hardware-behavior switch.** Fixed 16/14/30 ms, 100% wet, only Input Gain, a VU meter, and a cascade patch. Engineers who know the real box can use it like the real box.
3. **Correct dry/wet alignment and documented comb behavior.** Doubling and Haas use depend on it. Offer a "Haas helper" readout that shows the first-notch frequency and the notch spacing.
4. **Measured-unit captures (later).** Several hand-tuned units as selectable "serials," which is historically honest, since each unit was screw-tuned.
5. **Native, cross-platform, light on CPU, with CLAP from day one.**

## 4. Proposed control set

**Global**

| Control | Range | Default | Notes |
|---|---|---|---|
| Mode | Authentic / Extended | Authentic | Authentic locks the delays to 16/14 ms, hides feedback, and treats the output as wet-only unless Mix is unlocked |
| Routing | Dual Mono (L→A, R→B) / Widener (mono→A L, B R) / Cascade (A→B) / Ping-Pong* / M-S* | Widener | *Extended only |
| Model | Time Cube / Garden Hose* / Corrugated* / Long Coil* | Time Cube | *Clearly labeled as experimental |
| Color | A (original) / B (leveled) | A | Same idea as UA. Our implementation differs |
| Mix | 0–100% | 100% in Authentic, 30% in Extended | Dry path latency-compensated |
| Input Drive | −20…+20 dB | 0 dB (= +4 dBu nominal) | Drives the 1109, driver, and air nonlinearities |
| Output | ±15 dB | 0 dB | |
| Low Cut | Off, 20 Hz–2 kHz, 12 dB/oct | Off | Wet only |
| Bass / Treble | ±12 dB shelves | 0 | Wet only |
| Noise | Off / Authentic / Extra | Authentic | Authentic ≈ 72 dB S/N |
| Send | momentary or toggle | on | Same idea as UA: cut input, keep trails |
| Temperature | 15–30 °C | 20 °C | Trims delay and loss |
| Tuning | 0–100% | 80% | Calibration quality: scales echo gain and the documented residual faults (doc 06 §4). 100% = a fresh ±1 dB factory unit; low settings = a "Worn" unit near the ±2 dB limit |
| Unit | seed 1–99 | 1 | Picks positions for the real trimmers (MID RANGE, L.F., 10KC) and the residual notches and peaks |
| Quality | Eco (1× OS) / Normal (2×) / HQ (4×) | Normal | |

**Per channel (A, B), Extended mode only unless noted**

| Control | Range | Default |
|---|---|---|
| Delay | 5 ms–2.5 s, or note values with Sync | A = 16 ms, B = 14 ms |
| Coil length behavior | Fixed coil / Physical | Fixed coil |
| Feedback | 0–100% (loop-gain limited per Color) | 0 |
| Pan | L–R | A = L, B = R |
| Level | −inf…+10 dB | 0 dB (available in both modes) |
| Mute | on/off | off |

**Metering:** a VU per channel on the wet output, as on the hardware (meter transfer). Add a "notch helper" readout showing 1/(2τ) and 1/τ.

## 5. Naming and trademark caution

"Cooper Time Cube" is a UA trademark; UA's marketing writes it as Cooper® Time Cube. The plugin needs its own name, and should mention the original only descriptively ("inspired by the 1971 acoustic tube delay"). Hardware and patent facts can be cited freely. Don't copy UA or AudioThing UI artwork or presets, and don't fit our curves to their plugin outputs.

### Decision: product name is **Red Room Delay** by **Firewalk Audio** (2026-11-06)

- Replaces the working name Damn Fine Delay (also decided 2026-11-06). The code, UI and docs use the new names.
- **Identity:** bundle and CLAP ID `com.firewalkaudio.redroomdelay`, manufacturer code `Fwau`, plug-in code `Rr16`.
- **Optional subtitle:** "Model 1416," after the 14 and 16 ms lines and echoing the original's "920-16."
- **Branding guardrails:**
  - Don't use "Twin Peaks," character names, show logos, fonts, imagery, or theme music.
  - Keep any red-room, coffee or diner styling generic.
  - Refer to the hardware only descriptively.
- **To do before launch:**
  - Search USPTO trademarks for "Red Room" and "Firewalk" in the music software and audio equipment classes.
  - Secure the domain and social handles.
  - A quick web search on 2026-11-06 found no plug-in called Red Room Delay and no company called Firewalk Audio. Nearby names: Channel Robot's FireWalker synth plug-in and bdEnergy's RumbleRoom delay.
- **Shortlist kept as alternates:** Diane, Air Walk With Me, Trust No Hose, Spooky Action.
