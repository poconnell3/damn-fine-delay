"""Render before/after demos, response plots and a metrics summary.

Run:  python3 render.py [out_dir]
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

import signals
from engine import TAU_A, TAU_B, RedRoomDelay, Settings, db, digital_delay

FS = signals.FS
TAIL_S = 0.25
SERIES = ("#2a78d6", "#eb6834", "#1baf7a")          # categorical slots 1-3 (validated all-pairs)
INK, INK_2, GRID = "#0b0b0b", "#52514e", "#e4e3df"

VARIANTS = {
    "dry": "Dry source",
    "cube_doubler": "Red Room Delay doubler: dry + line A (16 ms) at -3 dB",
    "digital_doubler": "Plain digital doubler: dry + clean 16 ms delay at -3 dB",
    "cube_widener": "Red Room Delay widener: L = line A (16 ms), R = line B (14 ms), wet only",
    "digital_widener": "Plain digital widener: L = clean 16 ms, R = clean 14 ms",
    "cube_cascade": "Red Room Delay cascade A into B (30 ms), wet only",
    "cube_widener_hot": "Widener with Drive +14 dB (output trimmed back): driver, air and transformer grit",
}


def stereo(x: np.ndarray) -> np.ndarray:
    return np.stack([x, x], axis=1) if x.ndim == 1 else x


def render_variants(x: np.ndarray, cube: RedRoomDelay, hot: RedRoomDelay) -> dict[str, np.ndarray]:
    x = np.concatenate([x, np.zeros(int(TAIL_S * FS))])
    half = 10.0 ** (-3.0 / 20.0)
    return {
        "dry": stereo(x),
        "cube_doubler": stereo(cube.doubler(x)),
        "digital_doubler": stereo(x + half * digital_delay(x, FS, TAU_A)),
        "cube_widener": cube.widener(x),
        "digital_widener": np.stack([digital_delay(x, FS, TAU_A), digital_delay(x, FS, TAU_B)], axis=1),
        "cube_cascade": stereo(cube.cascade(x)),
        "cube_widener_hot": hot.widener(x),
    }


def write_audio(path: Path, y: np.ndarray, mp3: bool) -> None:
    peak = np.max(np.abs(y))
    if peak > 0.999:
        raise ValueError(f"{path.name} would clip (peak {db(peak):+.2f} dBFS)")
    sf.write(path.with_suffix(".wav"), y, FS, subtype="PCM_24")
    if mp3:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(path.with_suffix(".wav")),
                        "-codec:a", "libmp3lame", "-b:a", "256k", str(path.with_suffix(".mp3"))], check=True)


def _style(ax: plt.Axes) -> None:
    ax.grid(True, which="both", color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(INK_2)
    ax.tick_params(colors=INK_2, labelsize=9)
    ax.xaxis.label.set_color(INK_2)
    ax.yaxis.label.set_color(INK_2)


def impulse(n_s: float = 1.0, pre: float = 0.05) -> tuple[np.ndarray, int]:
    x = np.zeros(int((n_s + pre) * FS))
    x[int(pre * FS)] = 1.0
    return x, int(pre * FS)


def plot_responses(path: Path) -> None:
    x, start = impulse()
    fig, axes = plt.subplots(2, 1, figsize=(9, 7.5), constrained_layout=True)
    for ax, color, title in ((axes[0], "B", "Color B (leveled)"), (axes[1], "A", "Color A (original, with unit residuals)")):
        eng = RedRoomDelay(FS, Settings(color=color).linear())
        for (label, y), c in zip((("Line A, 16 ms", eng.wet_a(x)), ("Line B, 14 ms", eng.wet_b(x)),
                                  ("Cascade, 30 ms", eng.cascade(x))), SERIES):
            f = np.fft.rfftfreq(len(y), 1.0 / FS)
            m = db(np.fft.rfft(y))
            m -= np.interp(1000.0, f, m)
            ax.semilogx(f[1:], m[1:], color=c, linewidth=1.4, label=label)
        ax.axhspan(-0.7, 0.7, xmin=0, xmax=1, color=GRID, alpha=0.6, linewidth=0, label="Factory chart band (±0.7 dB)")
        ax.set_xlim(20, 20000)
        ax.set_ylim(-30, 4)
        ax.set_title(title, loc="left", color=INK, fontsize=11)
        ax.set_ylabel("Level re 1 kHz (dB)")
        _style(ax)
        ax.legend(frameon=False, fontsize=9, loc="lower left", labelcolor=INK_2)
    axes[1].set_xlabel("Frequency (Hz)")
    fig.suptitle("Red Room Delay: frequency response (comb ripple is the 3τ echo)", color=INK, fontsize=12, x=0.01, ha="left")
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_impulse(path: Path) -> None:
    x, start = impulse(0.12)
    eng = RedRoomDelay(FS, Settings(color="A").linear())
    fig, ax = plt.subplots(figsize=(9, 3.8), constrained_layout=True)
    t = (np.arange(len(x)) - start) / FS * 1e3
    for (label, y), c in zip((("Line A", eng.wet_a(x)), ("Line B", eng.wet_b(x))), SERIES):
        ax.plot(t, np.maximum(db(y), -100.0), color=c, linewidth=1.0, label=label)
    for tau, c in ((TAU_A, SERIES[0]), (TAU_B, SERIES[1])):
        ax.annotate(f"echo at 3τ = {3 * tau * 1e3:.0f} ms", xy=(3 * tau * 1e3 + 0.4, -60), xytext=(3 * tau * 1e3 + 4, -40 if c == SERIES[0] else -30),
                    color=INK_2, fontsize=9, arrowprops=dict(arrowstyle="-", color=INK_2, linewidth=0.6))
    ax.set_xlim(0, 70)
    ax.set_ylim(-100, 0)
    ax.set_xlabel("Time after input (ms)")
    ax.set_ylabel("Impulse response (dB)")
    ax.set_title("Impulse response: main arrival, then the end-reflection echo", loc="left", color=INK, fontsize=11)
    _style(ax)
    ax.legend(frameon=False, fontsize=9, labelcolor=INK_2)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def metrics() -> dict:
    import test_engine as t

    out: dict = {}
    lin = RedRoomDelay(FS, Settings(color="B").linear())
    x, start = impulse()
    for line in ("a", "b"):
        f, h = t.spectrum(getattr(lin, f"wet_{line}")(x))
        m = t.rel_db(f, h)
        band = (f >= 40) & (f <= 10000)
        out[f"color_b_{line}_span_db"] = [round(float(m[band].min()), 2), round(float(m[band].max()), 2)]
        out[f"color_b_{line}_db_at"] = {str(q): round(float(np.interp(q, f, m)), 1) for q in (20, 11000, 12000, 15000)}
    eng = RedRoomDelay(FS, Settings(noise="off", crosstalk=False))
    out["thd_pct_at_+4dBu"] = {str(int(q)): round(t.thd_pct(t.harmonics(eng.wet_a(t.tone(q, 4.0)), q)), 3)
                               for q in (100.0, 1000.0, 3000.0, 5000.0, 8000.0)}
    out["thd_pct_100Hz_by_level_dBu"] = {str(l): round(t.thd_pct(t.harmonics(eng.wet_a(t.tone(100.0, l)), 100.0)), 2)
                                         for l in (4, 10, 13, 16)}
    noisy = RedRoomDelay(FS, Settings(crosstalk=False))
    y = noisy.wet_a(np.zeros(3 * FS))[FS:]
    f, h = t.spectrum(y)
    rms = np.sqrt(np.sum(np.abs(h[f <= 15700]) ** 2) * 2.0) / len(y)
    out["sn_db"] = round(float(-18.0 - db(rms)), 1)
    return out


def main(out_dir: str = "out") -> None:
    out = Path(out_dir)
    if out.exists():
        shutil.rmtree(out)
    (out / "audio").mkdir(parents=True)
    mp3 = shutil.which("ffmpeg") is not None
    cube = RedRoomDelay(FS, Settings())
    hot = RedRoomDelay(FS, replace(Settings(), drive_db=14.0))
    for name, fn in signals.ALL.items():
        renders = render_variants(fn(), cube, hot)
        # One gain per source keeps the level differences between variants honest.
        gain = min(1.0, 10.0 ** (-1.0 / 20.0) / max(np.max(np.abs(y)) for y in renders.values()))
        for variant, y in renders.items():
            write_audio(out / "audio" / f"{name}_{variant}", gain * y, mp3)
    plot_responses(out / "frequency_response.png")
    plot_impulse(out / "impulse_response.png")
    summary = {"variants": VARIANTS, "metrics": metrics()}
    (out / "metrics.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary["metrics"], indent=2))


if __name__ == "__main__":
    main(*sys.argv[1:])
