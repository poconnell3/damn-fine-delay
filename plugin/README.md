# Damn Fine Delay (v0.2)

A real-time plug-in model of the 1971 two-line acoustic tube delay (16 ms and 14 ms lines, 30 ms cascade). The DSP core is plain C++20 with no JUCE dependency; JUCE wraps it as VST3, AU (macOS), CLAP and Standalone.

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release        # downloads JUCE 8.0.9 and clap-juce-extensions
cmake --build build                                            # builds and installs the plug-ins
ctest --test-dir build                                         # C++ spec tests (98 checks)
```

Each build copies the plug-ins into your user plug-in folders: on macOS `~/Library/Audio/Plug-Ins/` (`VST3`, `Components`, `CLAP`), on Linux `~/.vst3` and `~/.clap`, on Windows the VST3 goes to the Common Files `VST3` folder (needs an administrator prompt; copy the CLAP by hand). On macOS the build also restarts the Audio Unit registrar so Logic sees a rebuilt AU. Turn this off with `-DDFD_COPY_AFTER_BUILD=OFF`.

Options: `-DDFD_JUCE_PATH=/path/to/JUCE` uses a local JUCE checkout, `-DDFD_BUILD_CLAP=OFF` skips CLAP, `-DDFD_BUILD_PLUGIN=OFF` builds only the DSP core, tests and CLI, `-DDFD_BUILD_UI_SNAPSHOT=ON` adds `dfd_ui_snapshot`, which renders the editor to a PNG without a host. On Linux, JUCE needs the ALSA, X11, Xrandr, Xinerama, Xcursor, Xext, freetype and fontconfig development packages.

Null test against the Python reference (needs numpy and scipy, and the prototype folder on `PYTHONPATH`):

```
PYTHONPATH=../research/prototype:../research/tools python3 tests/null_test.py build/dfd_cli
```

## Layout

| Path | What it is |
|---|---|
| `src/dsp/Filters.h` | Biquads, cascades, polynomial roots, bilinear transform with pre-warping, analog prototypes |
| `src/dsp/GeneratedFilters.h` | Generated: minimal-order SEND/RECEIVE networks as functions of the trimmers, and the wall-loss shelf fits |
| `src/dsp/Line.*` | One acoustic line at the oversampled rate |
| `src/dsp/Oversampler.h` | Linear-phase half-band 1x/2x/4x |
| `src/dsp/Engine.*` | Two lines, routings, dry/wet, crosstalk, latency, parameter smoothing |
| `src/plugin/` | JUCE processor and editor |
| `src/plugin/ui/` | Rack face, controls (knobs, toggles, routing selector, pilot lamp) and meters |
| `tools/gen_filters.py` | Regenerates `GeneratedFilters.h` from the schematic and the tube model |
| `tools/dfd_cli.cpp` | Offline renderer used by the null test |
| `tools/ui_snapshot.cpp` | Renders the editor to a PNG after feeding it test audio |
| `tests/` | C++ spec tests and the Python null test |

## Signal chain (one line)

Drive, input transformer (flux saturation), SEND EQ, output RC, driver excursion limit, transducer pair, 15 Hz stub high-pass, air nonlinearity, wall loss, delay line with the first echo at 3 tau, pickup noise, RECEIVE EQ, output transformer, 1/Drive.

Every linear stage is an analog design (component values or physics) mapped with the bilinear transform. The transducer pair is defined as the inverse of the factory-trimmed electronics and the tube, times an 8th-order 0.1 dB Chebyshev low-pass at 11 kHz. Forward and inverse filters use the same pre-warping, so at factory trims the linear chain cancels exactly and the response is the band-edge filter alone, at any oversampling factor. The nonlinear stages still see the real pre-emphasis.

The delay is calibrated at prepare time: the wet path's group delay at 1 kHz equals tau plus the reported latency. The dry path is delayed by the reported latency.

## Parameters

Routing (Widener, Dual mono, Cascade), Color (A original / B leveled), Drive, Mix (default 40 %), Wet Only (ignores Mix and outputs the wet signal only, for sends), Level A, Level B, Output, Noise (Off / Authentic / Extra), Quality (1x / 2x / 4x, changes latency), Room temperature (changes the delay times).

Bypass keeps the reported latency, so switching it in and out doesn't shift the audio.

## Editor

A rack panel drawn at 1230 x 330 and scaled as a whole (drag the corner; the size is saved with the session).

- Left end: input meters. Right end: output meters. Peak meters on an IEC scale with peak hold and clip lamps (click a meter to reset). One meter for a mono bus, two for stereo. Labels follow the routing: inputs read L/R, or A/B in Dual mono (left feeds A, right feeds B); outputs read A/B, or L/R in Cascade.
- Left plate: the signal flow for the current routing, with the delay times at the current room temperature.
- VU meter: 0 VU = -18 dBFS, standard VU ballistics, reading output A or B (METER switch).
- ACTIVE lamp: lit while the host is sending audio through the plug-in. If it stays dark the host isn't processing the track (for example a record-armed track in Logic with input monitoring off).

## Licensing note

JUCE 8 is dual-licensed: AGPLv3, or the JUCE licence (Starter is free up to $20k a year of revenue; Indie and Pro above that). Choose before distributing. The plug-in codes in `CMakeLists.txt` (company "Damn Fine Audio", manufacturer code `Dfau`, plug-in code `Df16`) are placeholders.
