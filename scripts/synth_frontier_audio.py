"""Synthesize DOWNSTREAM frontier sound effects (original, no third-party audio)."""
import sys
import wave
from pathlib import Path
import numpy as np

SR = 44100
rng = np.random.default_rng(20261010)
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)


def t(sec):
    return np.arange(int(SR * sec)) / SR


def lowpass(x, cutoff):
    # one-pole low-pass, applied twice for a gentler slope
    a = np.exp(-2 * np.pi * cutoff / SR)
    for _ in range(2):
        y = np.empty_like(x)
        acc = 0.0
        for i, v in enumerate(x):
            acc = (1 - a) * v + a * acc
            y[i] = acc
        x = y
    return x


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def env(n, attack, release):
    e = np.ones(n)
    a = int(SR * attack)
    if a:
        e[:a] = np.linspace(0, 1, a)
    r = int(SR * release)
    if r:
        e[-r:] *= np.linspace(1, 0, r) ** 2
    return e


def save(name, x, peak=0.8):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    data = (x * 32767).astype(np.int16)
    with wave.open(str(out / f'{name}.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    print(name, f'{len(x) / SR:.2f}s')


# Alarm: two-tone klaxon, 1.6 s (looped by the outpost while alerted).
tt = t(1.6)
freq = np.where((tt % 0.8) < 0.4, 620.0, 470.0)
phase = 2 * np.pi * np.cumsum(freq) / SR
klaxon = np.sign(np.sin(phase)) * 0.6 + np.sin(phase * 2) * 0.25
klaxon = lowpass(klaxon, 2600) * env(len(tt), 0.01, 0.02)
save('S_FrontierAlarm', klaxon, 0.55)

# Collapse: low rumble + wood/metal cracks + debris.
tt = t(2.2)
rumble = lowpass(rng.standard_normal(len(tt)), 140) * np.exp(-tt * 1.6)
crack = np.zeros(len(tt))
for start in rng.uniform(0.0, 0.9, 9):
    i = int(start * SR)
    n = int(SR * 0.05)
    burst = highpass(rng.standard_normal(n), 1500) * np.exp(-np.linspace(0, 8, n))
    crack[i:i + n] += burst[: len(crack) - i] * rng.uniform(0.4, 1.0)
debris = highpass(rng.standard_normal(len(tt)), 800) * np.exp(-tt * 3.0) * (rng.random(len(tt)) > 0.97)
save('S_FrontierCollapse', rumble * 3.0 + crack * 0.8 + lowpass(debris, 4000) * 2.0, 0.85)

# Sandbag: heavy wet thud.
tt = t(0.45)
thud = np.sin(2 * np.pi * (90 - 50 * tt) * tt) * np.exp(-tt * 14)
slap = lowpass(rng.standard_normal(len(tt)), 1200) * np.exp(-tt * 30)
save('S_FrontierSandbag', thud + slap * 0.6, 0.8)

# Breach: water gushing through a levee cut (1.8 s, swelling).
tt = t(1.8)
gush = lowpass(rng.standard_normal(len(tt)), 900) * (0.4 + 0.6 * np.sin(np.pi * tt / 1.8))
bubbles = np.zeros(len(tt))
for start in rng.uniform(0, 1.7, 40):
    i = int(start * SR)
    n = int(SR * 0.04)
    f = rng.uniform(250, 700)
    b = np.sin(2 * np.pi * np.linspace(f, f * 1.6, n) * np.arange(n) / SR) * np.exp(-np.linspace(0, 6, n))
    bubbles[i:i + n] += b[: len(bubbles) - i]
save('S_FrontierBreach', gush * 2.0 + bubbles * 0.25, 0.75)

# Water loop: flowing stream, 4 s, seamless (crossfade the tail into the head).
total = 4.5
tt = t(total)
stream = lowpass(rng.standard_normal(len(tt)), 700) + highpass(lowpass(rng.standard_normal(len(tt)), 3500), 1200) * 0.25
mod = 0.75 + 0.25 * np.sin(2 * np.pi * 0.7 * tt) * np.sin(2 * np.pi * 0.23 * tt)
stream *= mod
fade = int(SR * 0.5)
loop = stream[: len(stream) - fade].copy()
w = np.linspace(0, 1, fade)
loop[:fade] = loop[:fade] * w + stream[-fade:] * (1 - w)
save('S_FrontierWaterLoop', loop, 0.6)

# Wreck: metallic crunch + sparks, for the digger going down.
tt = t(1.3)
clang = sum(np.sin(2 * np.pi * f * tt) * np.exp(-tt * d) for f, d in [(180, 5), (431, 7), (977, 9), (1630, 12)])
crunch = lowpass(rng.standard_normal(len(tt)), 2500) * np.exp(-tt * 6)
sparks = highpass(rng.standard_normal(len(tt)), 5000) * np.exp(-tt * 4) * (rng.random(len(tt)) > 0.9)
save('S_FrontierWreck', clang * 0.5 + crunch + sparks * 0.5, 0.85)
