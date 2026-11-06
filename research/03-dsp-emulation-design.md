# 03: DSP Emulation Design

Goal: a plugin that is **authentic by default**: two fixed lines (16 ms and 14 ms), a 30 ms cascade, 100% wet hardware behavior, and the hardware's coloration. Optional **extended** features sit on top without compromising that core.

## 1. Modeling approaches

| Approach | What it is | Pros | Cons | Use for |
|---|---|---|---|---|
| **A. Parametric physical model** (delay line + physically derived filters) | Each stage from physics (doc 02): tube loss filter, echo tap, transducer/cavity EQ, pre/post EQ, nonlinearities | Sample-rate independent. Every parameter means something (length, bore, temperature, reflection). Extended modes come almost free. Cheap on CPU. | Accuracy is limited by unknowns (tube bore, individual transducer responses, exact echo level) until calibrated. The EQ curves are now known (doc 06). | **Core engine** |
| **B. Measured IR convolution** | Sweep a real unit and convolve with the ~70 ms IR | Exact linear response of one specific unit | No nonlinearity. Fixed delay. Needs hardware access. Partitioned convolution for 2–4 channels × 70 ms is affordable but not free. | Calibration reference, possibly a "captured unit" mode |
| **C. Hybrid** | Parametric delay/echo core plus a low-order IIR fit (or short min-phase FIR) of the measured "color" | Measured tone with parametric flexibility | Needs measurement data | **Target once a unit is measured** |
| **D. Full digital waveguide** | Two-way delay lines with scattering junctions at the driver, T-block, and stub | Physically complete: all reflection orders, stub interaction, "hose" models with variable termination | More state and more tuning. Adds little over (A) for the authentic model, since reflections beyond the first are negligible. | Experimental "Garden Hose / Long Coil" models |

**Recommendation:** build A now, structured so the measured color filters of C drop in later. Keep D for experimental models.

## 2. Signal flow (per channel, authentic mode)

```
in ─► [Input stage: 1109 in-xfmr + amp]  (drive, LF xfmr sat, gentle tanh)
   ─► [Pre-EQ]  SEND network: bass cut + 10.4 kHz pre-emphasis     (~3 SOS, doc 06)
   ─► [Driver]  cartridge-as-speaker: BP + resonances, excursion soft-limit (2× OS)
   ─► [Air nonlinearity]  y = x + k_nl · d/dt(x²)                 (2× OS, optional)
   ─► [Delay line]  write head
         ├─ read @ τ          ─► H_w(z)            (tube wall loss, min-phase)          ─┐
         └─ read @ 3τ         ─► H_echo(z)·g_echo  (≈ R(f)·H_w³, extra low-pass)        ─┤ sum
   ─► [T-block / Helmholtz residuals]  peaks/dips (Color A) or flat (Color B)  ◄────────┘
   ─► [Pickup]  cartridge response
   ─► [+ Noise]  −70 dB re +4, shaped by what follows
   ─► [Post-EQ]  RECEIVE network: LF boost + 10.4 kHz resonant boost (~3 SOS, doc 06)
   ─► [Output stage: 1109 amp + out-xfmr]
   ─► [Crosstalk]  HF-rising leak from the other line's send (−45 dB @1 kHz → −25 dB @10 kHz)
out
```

**Collapsed (low-CPU) variant.** Every stage between the nonlinearities is LTI. So Pre-EQ × Driver × H_w × T-block × Pickup × Post-EQ can be merged into one "color" filter of about 6–10 biquads, plus the echo tap filter. Use this when the air nonlinearity is off. Keep the split topology when the nonlinearity or noise is on, because those depend on where they sit relative to the EQ.

## 3. Stage details

### 3.1 Delay line (fractional)

- One circular buffer per channel, sized for the maximum extended delay (e.g. 2.5 s at 192 kHz, about 480k samples × 4 bytes ≈ 1.9 MB per channel; allocate in `prepareToPlay`).
- **Fixed/authentic delay:** use a **1st-order Thiran allpass** for the fraction. It has flat magnitude, is cheap, and keeps comb notch positions exact. The fraction only changes on sample-rate change. `juce::dsp::DelayLine<float, DelayLineInterpolationTypes::Thiran>` provides it, but JUCE documents Thiran as stateful and unsuitable for fast modulation.
- **Variable delay (extended mode, sync, modulation):** use **Lagrange 3rd order** (JUCE `Lagrange3rd`) or 4-point Hermite. Linear interpolation low-passes the signal, which would double up with the tube's own HF loss.
- **Delay changes:** offer two behaviors:
  - **Glide:** smoothed read position. Pitch-shifts like tape. Smooth over 50–300 ms.
  - **Jump:** dual read-head crossfade over 10–30 ms. Behaves like swapping hoses, with no pitch artifact.
- **Echo tap:** a second read at 3τ (authentic). In extended mode with long delays, scale it by the *reference* coil length instead of the user delay, so the "coil sound" is preserved, which matches UA's behavior. Doc 04 covers the physical-scaling option.

### 3.2 Tube wall-loss filter H_w

- **Target:** H_w(ω) = exp(−(1+j)·s·α(ω)·L). Here α is Kirchhoff (doc 02 §3) and s is the loss scale: 1.0 = ideal, about 1.6 = Cooper-measured. It is minimum-phase.
- **Design: Abel/Smyth/Smith DAFx-03.** Use a **cascade of first-order shelf filters** with unity DC gain. Coefficients are closed-form functions of radius, length, and fs. At fifth order the fit is accurate to a fraction of a dB across the audio band. It can be interpolated in real time (coefficients interpolated linearly or exponentially between computed values), so length and bore can be live parameters.
- **Fallback design** if we don't reproduce their closed-form fit: fit offline per parameter grid point. Take the target magnitude, compute the minimum phase via the real cepstrum, then fit with Prony or the Steiglitz–McBride method. Or do a least-squares fit of N first-order shelves (frequencies log-spaced, gains free). Store coefficient tables over (L, a, fs) and interpolate.
- **Authentic mode:** the post-EQ largely cancels H_w in magnitude **and** phase (see doc 02 §4). Implement H_w explicitly only in the split topology. In the collapsed topology, only the *residual* (EQ mismatch) remains inside the color filter.

### 3.3 Echo path (fast wiggles)

- Gain g = r_rx · r_src, with defaults r_rx = 0.1 and r_src = 0.9. These fit the factory chart's ≲1 dB LF ripple (doc 06 §5). Refine with measurements.
- Filter: H_w² relative to the direct path (two extra traversals), times a reflection-coefficient shape R(f). R(f) should rise at LF, where the quarter-wave stub/plug tuning is least effective.
- A cheap version is 2–3 first-order shelves or a 2-pole low-pass at about 200–600 Hz with a gentle slope. Check it against the model's table at current defaults (12.7 mm bore, Cooper loss law): about −28 dB at 100 Hz, −43 dB at 1 kHz, negligible above 5 kHz. Calibrate against a sweep capture of a real unit when one is available; the IR pack we planned to use is no longer sold (doc 07 §5).
- **Cascade mode** produces two echo taps automatically when A feeds B. Each channel's echo stays inside its own line, which gives the 58/62 ms pair. Their beat null sits at about **120 Hz**, not the naive 1/(2·4 ms) = 125 Hz: the A echo carries two extra passes of the *longer* tube, and that extra wall-loss phase pulls the null down by ~5 Hz (prototype and physics model agree).

### 3.4 Transducers, cavities, and "Color"

- **Transducer pair (R50 driver + R70 receiver):** start from the curve implied by EQ × tube = flat at the current defaults (doc 07 §1): about −5 dB at 31 Hz, +2 dB at 200 Hz, −1 to −2 dB from 2 to 5 kHz, −2.3 dB at 7 kHz, and −10.6 dB at 10 kHz, re 1 kHz. Fit it with 3–4 biquads. Put most of the HF rolloff on the driver side, so the pre-emphasis feeds the air nonlinearity at the right level. Use the published R50 (Shure 561) curve as the HF texture seed: the +6 dB presence peak near 4 kHz is mostly cancelled by the factory "MID RANGE" trim and the Tee screws, and the −4.5 dB notch near 7 kHz becomes the residual "icicle." Do not use its free-field bass rolloff.
- **Color A (original):** the transducer pair, the true electronic EQ (§3.5), the tube loss, and documented residuals from the production procedure:
  - the 6.7–7.2 kHz "icicle" notch (0.5–1 dB on UREI's special R50 diaphragm)
  - the slight ~9 kHz dip
  - a residual 3 kHz bump of about ±0.5 dB
  - the steep cutoff above about 11 kHz
- The net result must sit within about ±0.7 dB from 40 Hz to 10 kHz, matching the factory chart for serial 117.
- **Color B (leveled):** the same chain with the residual notches and bumps zeroed.
- **Unit variation (seeded, stored in state):** draw from the documented faults instead of generic randomness:
  - the icicle notch, 0.5–3 dB at 6.7–7.2 kHz
  - an 8–8.5 kHz peak (receiver pressed in too far), 0–1.5 dB
  - 200 Hz and 400 Hz air-leak dips, 0–1 dB
  - LF stub wobble
  - the three trimmer positions (MID RANGE, L.F., 10KC)
  - a **"Re-capped (SM57)"** variant: many surviving units run modern SM57 cartridges without retuning the Tee screws (doc 07 §2). Model it as a brighter driver (no R50 7 kHz notch, an SM57-style presence rise at 5–6 kHz) through the unchanged factory EQ, which leaves a slight 5–10 kHz excess.

  Keep each unit within ±1 dB, with "Worn" settings allowed up to the ±2 dB spec.
- Two cartridges in series, one used backwards: by reciprocity, a dynamic element's response as a driver resembles its response as a receiver. The two responses roughly multiply, so resonances are emphasized about twice in dB.

### 3.5 Pre-EQ and post-EQ

- **Transcribed from schematic D11688 (doc 06 §2).** Use the real component networks:
  - **Send:** G = 1 + Z_f/Z_g, with Z_f = 3.9k‖1nF and Z_g = (25mH+9.4nF+120) ‖ (68nF+120) ‖ (1.8k + R4 + (10k‖0.15µF)). That is a 4th–5th-order rational function.
  - **Receive:** a T-network feedback with a 25mH+9.4nF+R23 shunt and R21-adjustable LF.
  - Reference responses are in `tools/ctc_eq_networks.py`.
- **Implementation:**
  1. Build the analog transfer functions as polynomial ratios (expand the impedances symbolically or with numpy `poly` arithmetic).
  2. Factor them into second-order sections.
  3. Map each section with the **bilinear transform, pre-warped at 10.4 kHz** so the LC resonance lands exactly. At 44.1 kHz an unwarped bilinear would move the 10.4 kHz peak down to about 9 kHz.
  4. Recompute the coefficients whenever a trimmer parameter changes (cheap: a few sections).
- **Measured guidance from the prototype work (2026-11-06):**
  - **Reduce to minimal order first.** Naive numpy polynomial expansion of the receive network gives order 15 with near-cancelling pole/zero pairs, and `zpk2sos` fails on it ("complex value with no matching conjugate"). Symbolic cancellation (sympy) gives the true orders: **send 5, receive 6**. Derive those offline and ship the closed-form coefficient expressions.
  - **Warping errors cancel, so 2× is enough.** On its own, a bilinear EQ pre-warped at 10.4 kHz is off by 0.47 / 0.27 dB (send / receive) at 96 kHz and 0.12 / 0.07 dB at 192 kHz. But the transducer pair is defined as the inverse of the same networks, mapped with the same transform and warp, so the forward and inverse filters cancel exactly at any rate (§9). Only the nonlinear stages and the noise see the warp, which is inaudible. The plug-in runs 2× by default.
  - The prototype's FIR realization (exact analog response on an FFT grid, 8th-order 24 kHz guard low-pass) is accurate to ≤0.007 dB and is the reference the IIRs are tested against.
- **The pre/post split is now known:** about 60% of the HF boost (in dB) is pre-emphasis; bass is cut pre and restored post. This places the air nonlinearity and the noise correctly with no guesswork.
- **The send output RC** (120 Ω / 0.1 µF into the ~200 Ω driver) adds −1 dB at 10 kHz and −3 dB at 20 kHz. Include it.

### 3.6 Nonlinearities (all 2× oversampled; 4× in an HQ option)

1. **Input/output transformer + 1109 amp:** a flux-domain saturator. Integrate (leaky, with the corner around 5–10 Hz), apply a soft clip (tanh or a cubic soft clip), then differentiate. That gives LF-heavy, level-dependent saturation, which is cheap and convincing. Add a tiny static asymmetry for some 2nd harmonic. Keep it very clean at nominal (the 1109 is typically <0.05% midband); it should matter only with Drive or at LF. The prototype uses a 4/40 Hz flux shelf and 12·tanh(x/12): about 0.5% at 30 Hz, +24 dBu.
2. **Air nonlinearity (Cooper eq. 4):** y = x + k_nl · d/dt(x²). Use a first-difference derivative at the oversampled rate and apply it to the pre-emphasized signal before H_w. **Calibrate at the output, not inside the tube.** The send pre-emphasis raises HF level in the tube and the receive EQ then boosts the 2nd harmonic relative to the fundamental, so output THD peaks near 5 kHz. A k_nl giving 0.2% H2 at 1 kHz inside the tube gives **2.4% at 5 kHz at the output**, which breaks the spec's <1% "at all levels up to +4 dBm out." The prototype therefore sets k_nl = 0.00076/(A_nom·2π·1 kHz): **0.85% worst case (5 kHz) and 0.05% at 1 kHz at +4 dBu**, scaling with drive. Still an **assumption** until we have measurements. Since D ∝ length, scale k_nl by L/L_ref in physical mode.
3. **Driver excursion:** excursion ∝ ∫velocity, so it is LF-dominant. Integrate, soft-limit, differentiate, the same pattern as the transformer, but with a higher corner (~200 Hz). The factory checks for the R50 diaphragm **striking its resonator cover plate** at 100 Hz, +10 dBm output. So model an **asymmetric, fairly hard** limit (one polarity only, like hitting the plate) whose onset is around +10 to +14 dBm at 100 Hz. Start at +12 dBm and use a "Worn" unit setting to lower it. The send EQ's 6–9 dB bass cut is what normally keeps this limit out of reach.

**Oversampling:** v0.1 uses its own linear-phase half-band FIR stages (31 samples of latency at 2×, 39 at 4×), so the latency is constant across frequency and the delay calibration is exact. IIR polyphase filters would cut latency to a few samples at the cost of frequency-dependent delay. Report the latency with `setLatencySamples`, **and delay the internal dry path by the same amount**. Haas and comb behavior depend on the exact dry/wet offset, so even a few samples of misalignment shift the notches.

### 3.7 Noise and crosstalk

- **Noise:** inject white Gaussian noise at the receive-amp input, *before* the receive EQ. The EQ then shapes it into the authentic band-limited hiss centered near 10 kHz. Scale it so output S/N is 70–75 dB re +4 dBu over a 15.7 kHz bandwidth. Use an independent generator per channel. Offer a "Noise" switch, on by default at the authentic level.
- **Hum (optional, off by default):** 60/120/180 Hz at about −85 dB, a nod to the 25 ft mic cables.
- **Crosstalk:** follow the production limits, **−45 dB at 1 kHz rising to −25 dB at 10 kHz**, about a +20 dB/decade high-pass shape. Take the leak from the other channel's **send** signal, which is the high-level, pre-emphasized one, and inject it at this channel's receive node. It then arrives undelayed by the other tube, which is plausible for electrical coupling in the shared cable runs. Default to about 6 dB better than those limits, since they are pass/fail limits and real units beat them.

### 3.8 Temperature

A static parameter (15–30 °C, default 20 °C). It scales c, which changes τ (−0.17%/°C) and slightly changes α (∝ 1/c). Don't modulate it in authentic mode. An extended "Drift" option can add very slow random-walk temperature (time constant of minutes, ±0.3 °C) for subtle movement.

## 4. Extended features (not in hardware)

- **Feedback (Decay):** feed the channel output (after color) back into its input stage. Put the 1109 saturator *inside* the loop so runaway settles into soft saturation. Add a DC blocker at about 5 Hz in the loop. Clamp the loop gain so that max|H_loop| × fb < 1 at all frequencies in Color A, because the residual peaks can otherwise push one band into self-oscillation. Repeats get progressively darker and more "hosey," with the echo ripple compounding. That is the desired character.
- **Delay range:** 5 ms to 2.5 s, matching UA, plus tempo sync. Two choices of "coil sound vs length":
  - **Fixed coil (default):** the coloration of the reference coil, at any delay. This is UA's behavior.
  - **Physical length:** loss ∝ L, so longer delays get darker, and the echo tap moves to 3τ. Above about 50 ms, cap the echo or turn it off, or long delays get a quiet triple-time ghost.
- **Ping-pong, M/S, mono-sum routings** (AudioThing-style).
- **Hose models** (DIY-inspired; tagged as non-authentic):
  - *Garden hose:* soft plastic wall, so add Cooper's "anomalous" 1–5 kHz loss (a broad dip of −6 to −12 dB), a wider 400 Hz–5 kHz effective band, and stronger reflections (r_rx about 0.6).
  - *Corrugated:* periodic ridges give Bragg-like scattering, which shows up as narrow notches at f_n ≈ n·c/(2·pitch) and as an extra rough texture.
  - *Copper line:* Ray Lubow's 1950s copper-tube "fake stereo" box (doc 07 §4). Use a smooth √f loss law (loss_scale ≈ 1.0–1.2), no 1–5 kHz anomaly, and slightly higher wall reflection for a touch more diffusion. Delay about 25 ms.
- **Tone:** Bass and Treble shelves plus a 12 dB/oct HPF (20 Hz–12 kHz) on the wet signal only, as in UA.

## 5. Real-time engineering checklist

- Allocate everything in `prepareToPlay`. Nothing allocates, locks, or logs on the audio thread.
- Smooth every continuous parameter: `SmoothedValue`, multiplicative for gains, 20–50 ms. Recompute filter coefficients per block or per 32 samples, interpolating coefficients for the shelf cascades.
- Use FTZ/DAZ (`juce::ScopedNoDenormals`) and DC-block the feedback loops.
- Keep the CPU budget small. The authentic collapsed topology at 48 kHz stereo is about 2 delay lines, ~20 biquads, a 1st-order Thiran, and 2× oversampled saturators. A modern core should run 100+ instances.
- Bus layouts: mono→mono (A only, or cascade), mono→stereo (A→L, B→R, the classic widener), stereo→stereo (L→A, R→B dual mono). Pan is disabled for mono→mono, as UA does.
- Formats: JUCE 8 → VST3, AU, AAX, plus CLAP via clap-juce-extensions. Store state as XML/ValueTree with a version number for migrations.
- Tail: report `getTailLengthSeconds()` as delay × feedback-dependent decay. With feedback off it is about 3τ + 50 ms.

## 6. Calibration plan

**If a real unit is available** (rental, collector studio, or service shop):

1. Measure each channel and the cascade with an exponential sine sweep at −20, −10, 0, +4, +10, +16 dBu. Capture 200 ms after the main arrival to get the echo.
2. Extract the linear IR (from the lowest level) and the harmonic IRs (Farina method) to get THD versus frequency versus level.
3. Fit:
   - main delay (sub-sample)
   - echo delay and level and its low-pass
   - the color as a minimum-phase fit (6–10 biquads)
   - nonlinearity constants (k_nl, transformer drive)
4. Record the noise floor (input terminated) and the crosstalk (drive A, record B).
5. Note the room temperature, since it sets the actual delay.

**Without a unit (the actual situation; decided 2026-11-06: no physical unit or bench access is available, and the only commercial IR pack is gone):** tune to the factory spec envelope (±2 dB 40 Hz–10 kHz, THD, S/N) and to physics. The factory chart still bounds the echo (doc 07 §5). Use well-documented hardware recordings as listening references. **Don't** reverse-engineer competitor plugins' outputs to clone their curves. Use them only for general A/B listening.

## 7. Verification tests (map to unit tests)

| Test | Pass criterion |
|---|---|
| Delay accuracy at 44.1 / 48 / 88.2 / 96 / 192 kHz | Group delay at 1 kHz within ±0.05 samples of τ·fs (+ OS latency compensated) |
| Echo position | Echo peak at 3τ ± 0.5 ms; relative level matches the model table within ±2 dB |
| Ripple spacing | Ripple period of the equalized magnitude = 1/(2τ) ± 1% |
| Cascade nulls | First beat null matches the physics model (≈120 Hz) ± 2 Hz |
| Frequency response (Color B) | Within ±0.7 dB from 40 Hz to 10 kHz (factory chart, serial 117); steep rolloff above about 11 kHz (exact slope still to be measured) |
| EQ networks | Send/receive SOS responses match `ctc_eq_networks.py` within ±0.1 dB from 20 Hz to 20 kHz at 44.1–192 kHz |
| THD | ≤ 0.5% at +4 dBu, 1 kHz; < 1% at +4 dBu from 100 Hz to 8 kHz (prototype: 0.05% / 0.85% worst at 5 kHz). 100 Hz driver strike: < 0.1% at +4 dBu, > 3% at +16 dBu |
| Crosstalk | Feed B, read A: −45 dB ± 1.5 at 1 kHz, −25 dB ± 1.5 at 10 kHz |
| Noise | Output S/N between 70 and 76 dB (unweighted, 15.7 kHz BW) |
| Linear-core null vs reference | Python model IR vs plugin IR (NL and noise off): error < −45 dB from 40 Hz to 2 kHz (−39 dB cascade), level within ±0.1 dB to 10 kHz; band-edge phase above 2 kHz reported, not failed (§9) |
| Automation stress | Sweeping delay, feedback, and color produces no clicks, NaN/Inf, or denormal CPU spikes |
| State | Save/load round-trips exactly; old versions load |

`prototype/test_engine.py` implements the delay (44.1–96 kHz), echo, ripple, cascade, frequency response, EQ network (FIR realization, 20 Hz–16 kHz), THD, crosstalk and noise rows; all 19 tests pass. The C++ engine passes the same rows plus the null, automation and state rows (§9).

## 8. Python prototype (golden reference)

`research/prototype/` holds the offline engine (`engine.py`), the exact EQ networks (`analog.py`), synthesized test program, demo renderer and the spec tests. See its README for the chain, calibration choices and measured results. Decisions it settled:

- **Transducer pair = implied inverse.** Its magnitude is the inverse of factory-trimmed EQ × tube loss, times the band-edge low-pass (8th-order Chebyshev I, 0.1 dB, 11 kHz), realized minimum-phase. Because every stage is minimum-phase, it also cancels the EQ and wall-loss phase, so the direct path is a near-pure delay. Moving a trimmer away from factory position then becomes a real tone change.
- **Split the 15 Hz stub high-pass out of the transducer FIR** and run it as an IIR. Inside a cepstral FIR it cost 0.4 dB at 40 Hz from truncation.
- **Calibrate the delay on the realized chain.** Trim the bulk delay so the measured group delay at 1 kHz equals τ. The analytic wall-loss group delay differs from a realized √f kernel by ~0.4 samples at 96 kHz, because the kernel has a t^−1.5 tail. The C++ shelf-cascade H_w will need the same trim, measured at prepare time.
- **Noise reference.** 72 dB S/N is taken re nominal (+4 dBu). If the spec meant re maximum output, noise would be ~14 dB higher.

## 9. Real-time engine v0.1 (C++, `plugin/`)

Built 2026-11-06. The DSP core is plain C++20 with no JUCE dependency; JUCE 8.0.9 wraps it as VST3, CLAP and Standalone (AU on macOS). Source is in the project under `plugin/`; see its README.

**Architecture decisions**

- **Everything linear is an analog design mapped by the bilinear transform.** SEND and RECEIVE come from `tools/gen_filters.py`, which reduces the schematic symbolically (sympy) to its minimal orders, 5 and 6, as functions of the three trimmers. It checks the result against `ctc_eq_networks.py` (error < 1e-12 dB) and confirms minimum phase over the whole trimmer range, so the inverses are stable. The plug-in finds the roots at prepare time (Aberth iteration).
- **Wall loss:** 12 first-order shelves per tube, fitted to |H_w| from 5 Hz to 30 kHz. Magnitude error is ≤ 0.006 dB from 20 Hz to 20 kHz. The fit's phase differs from the √f kernel by an almost constant 8–9 µs, a pure delay that the calibration absorbs.
- **Transducer pair = exact inverse.** It is the inverse of the factory-trimmed SEND, RC, RECEIVE and wall loss, times an 8th-order 0.1 dB Chebyshev I low-pass at 11 kHz. Forward and inverse filters share the same mapping, so at factory trims the linear path is the band-edge filter alone. A trap found on the way: the RC's inverse is improper on its own. Mapped alone, its padding zero lands at z = 0 instead of cancelling the RC's z = −1 zero, which cost 0.33 dB at 9 kHz. It is now mapped together with the low-pass.
- **Echo** comes from a second tap on the same delay line at τ_bulk + 2τ, filtered by the wall loss twice.
- **Delay:** first-order Thiran taps. At prepare time the bulk delay is set so the wet path's group delay at 1 kHz equals τ plus the reported latency; the dry path is delayed by the reported latency. Oversampling is linear-phase half-band FIR: 31 samples of latency at 2×, 38.5 at 4× (reported as 39, with the half sample added to the wet path).
- **Color A/B** crossfades a residual-bell section over 20 ms, gliding the delay taps so the 1 kHz delay stays exact.

**Verification**

- **95 C++ checks pass**, also under ASan and UBSan. They cover delay accuracy (≤ 0.01 samples at 44.1–192 kHz, 1×/2×/4×), Color B −0.52…+0.43 dB, the 7 kHz notch, echo at 48.3 / 42.2 ms (−27.9 / −42.9 dB), the cascade null at 120.5 Hz, THD 0.055% at 1 kHz, S/N, crosstalk, dry alignment and automation stress.
- **pluginval strictness 10 passes** on the VST3.
- **Null test vs the Python reference passes:** error ≤ −45 dB from 40 Hz to 2 kHz (−39 dB for the cascade), level within 0.1 dB to 10 kHz. Above 2 kHz the two differ in phase near the band edge. Against the exact analog target at 9 kHz, the engine is off by 0.03 rad at 4× and 0.11 rad at 2×, the Python FIR by 0.09 rad; all are under 2 µs of group delay.

**Reference fixes made along the way** (in `research/prototype/`):

- The band edge is now the same Chebyshev filter (it was a clamp plus −36 dB/oct).
- Noise no longer scales with Drive (the hiss is fixed inside the unit, so driving harder and trimming back lowers it).
- The delay calibration and its test use a phase fit over 800–1200 Hz. The old ±1 Hz difference was biased about 5 µs by phase ripple from long low-level tails.

**Not in v0.1:** Extended mode (feedback, variable delay, ping-pong, M/S), the hose models, Unit seeds and trim controls, Low Cut and tone shelves, Send, metering, and a designed UI. The current editor is a plain functional panel.
