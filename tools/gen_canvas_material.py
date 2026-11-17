#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Felucca Canvas: the granular source material, synthesized here (no recordings, nothing third-party).

Three sets of four materials, each one a sound that changes along its length, so a grain's POS (and SCAN, SPRY, the
LFO on the place) moves through it:

  NATURE (16 kHz)   RAIN    a downpour building, then thinning to single drops
                    CREEK   bubbling water over a low rush, getting busier
                    BIRDS   wind, then calls: chirps, trills, a call and its answer
                    NIGHT   crickets, frogs answering, a hum under it
  DREAM (22.05 kHz) GLASS   FM glass bells, a chord arriving note by note, shimmering
                    DRIFT   a wide detuned chord, its filter opening then closing
                    VOX     a sung vowel: a -> e -> i -> o -> u, vibrato growing
                    LIQUID  a resonant sweep, its wobble speeding up, ring-mod sparkle
  GLITCH (22.05 kHz) DATA   a modem's worth of bursts: crushed noise, square bleeps, sample-and-hold
                    CIRCUIT chaotic feedback FM, a logistic-map oscillator stepping
                    TAPE    an arpeggio on a failing tape: wow, flutter, drop-outs, speed jumps
                    STATIC  tuning a radio: hiss, heterodyne whistles, a formant voice, morse

Pitched material is in C (C4 = 261.6 Hz) so a grain's PTCH plays in key. Every material is normalised, starts and ends
with a short fade, and is deterministic (fixed seeds): the same bytes on every build.

  gen_canvas_material.py OUTDIR   -> OUTDIR/<SET>/<n> <NAME>.wav (16-bit mono) and a list on stdout
"""
import sys
import wave
from pathlib import Path

import numpy as np

C4 = 261.6256

# set, rate, [(name, seconds)]
SETS = [
    ("NATURE", 16000, [("RAIN", 1.45), ("CREEK", 1.45), ("BIRDS", 1.6), ("NIGHT", 1.45)]),
    ("DREAM", 22050, [("GLASS", 1.25), ("DRIFT", 1.25), ("VOX", 1.15), ("LIQUID", 1.15)]),
    ("GLITCH", 22050, [("DATA", 0.95), ("CIRCUIT", 0.95), ("TAPE", 1.05), ("STATIC", 0.95)]),
]


# ---- helpers

def rng(name):
    return np.random.default_rng(abs(hash_str(name)) % (2 ** 32))


def hash_str(s):
    h = 2166136261
    for c in s.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def onepole_lp(x, fc, sr):
    """one-pole low-pass, fc scalar or per sample"""
    fc = np.broadcast_to(np.asarray(fc, dtype=float), x.shape)
    a = 1.0 - np.exp(-2 * np.pi * fc / sr)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s += a[i] * (x[i] - s)
        y[i] = s
    return y


def svf(x, fc, q, sr, mode="lp"):
    """Chamberlin state-variable filter, fc per sample (Hz), q damping 0..2 (lower = more resonant)"""
    fc = np.broadcast_to(np.asarray(fc, dtype=float), x.shape)
    f = 2 * np.sin(np.pi * np.minimum(fc, sr / 6) / sr)
    lo = bp = 0.0
    y = np.empty_like(x)
    for i in range(len(x)):
        hi = x[i] - lo - q * bp
        bp += f[i] * hi
        lo += f[i] * bp
        y[i] = lo if mode == "lp" else bp if mode == "bp" else hi
    return y


def pink(n, r):
    w = r.standard_normal(n)
    f = np.fft.rfft(w)
    k = np.arange(len(f))
    k[0] = 1
    f /= np.sqrt(k)
    return np.fft.irfft(f, n)


def env_ad(n, a, d, sr):
    t = np.arange(n) / sr
    return np.minimum(t / max(a, 1e-4), 1.0) * np.exp(-np.maximum(t - a, 0) / max(d, 1e-4))


def place(out, x, at):
    at = int(at)
    if at >= len(out):
        return
    m = min(len(x), len(out) - at)
    out[at:at + m] += x[:m]


def ramp(n, *pts):
    """piecewise-linear curve over n samples through (fraction, value) points"""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return np.interp(np.linspace(0, 1, n), xs, ys)


def osc_phase(freq, sr):
    return 2 * np.pi * np.cumsum(np.broadcast_to(np.asarray(freq, dtype=float), np.shape(freq) or (1,))) / sr


def saw(ph):
    return ((ph / (2 * np.pi)) % 1.0) * 2 - 1


def blsaw(f, t, sr, phase=0.0, top=0.42):
    """a band-limited saw (additive: the harmonics below top * sr), f a constant"""
    x = np.zeros(len(t))
    for h in range(1, int(top * sr / f) + 1):
        x += np.sin(2 * np.pi * f * h * t + h * phase) / h
    return x * (2 / np.pi)


def soft(x, k=1.5):
    return np.tanh(k * x) / np.tanh(k)


# ---- NATURE

def rain(n, sr, r):
    t = np.arange(n) / sr
    dens = ramp(n, (0, 0.35), (0.3, 1.0), (0.6, 0.8), (1, 0.15))
    bed = svf(pink(n, r), 2500, 1.2, sr, "bp") * 0.5 + onepole_lp(r.standard_normal(n), 5000, sr) * 0.12
    out = bed * dens
    k = 0
    while k < n:                                  # drops: short pitch-falling blips, sparser at the end
        k += int(sr * r.exponential(0.006 / max(dens[min(k, n - 1)], 0.05)))
        if k >= n:
            break
        f0 = r.uniform(1400, 4200)
        m = int(sr * r.uniform(0.008, 0.03))
        tt = np.arange(m) / sr
        f = f0 * (1 + 0.6 * np.exp(-tt * 180))
        d = np.sin(2 * np.pi * np.cumsum(f) / sr) * np.exp(-tt * r.uniform(90, 250)) * r.uniform(0.1, 0.5)
        place(out, d, k)
    for at in (0.72, 0.8, 0.86, 0.91, 0.95):        # the last single drops: bigger, ringing plinks on a leaf
        m = int(sr * 0.09)
        tt = np.arange(m) / sr
        f0 = r.uniform(900, 1800)
        d = (np.sin(2 * np.pi * f0 * tt * (1 + 0.25 * np.exp(-tt * 60))) * np.exp(-tt * 45)
             + 0.4 * np.sin(2 * np.pi * f0 * 2.7 * tt) * np.exp(-tt * 90))
        place(out, d * 0.8, at * n)
    return out + 0 * t


def creek(n, sr, r):
    busy = ramp(n, (0, 0.4), (0.5, 1.0), (1, 1.4))
    rush = svf(pink(n, r), ramp(n, (0, 500), (1, 900)), 1.4, sr, "bp") * 0.6
    out = rush * ramp(n, (0, 0.6), (1, 1.0))
    k = 0
    while k < n:                                  # bubbles (Minnaert): a pitch rising as they rise, decaying
        k += int(sr * r.exponential(0.012 / busy[min(k, n - 1)]))
        if k >= n:
            break
        f0 = r.uniform(280, 1400) * (1.0 if r.random() < 0.8 else 0.5)
        m = int(sr * r.uniform(0.02, 0.07))
        tt = np.arange(m) / sr
        f = f0 * (1 + tt * r.uniform(4, 12))
        b = np.sin(2 * np.pi * np.cumsum(f) / sr) * np.exp(-tt * r.uniform(40, 110)) * r.uniform(0.15, 0.6)
        b *= np.minimum(tt * 2000, 1)
        place(out, b, k)
    return out


def birds(n, sr, r):
    wind = svf(pink(n, r), 300 + 250 * np.sin(np.linspace(0, 5, n)) ** 2, 1.6, sr, "bp")
    out = wind * ramp(n, (0, 1.0), (0.3, 0.6), (1, 0.4)) * 0.9

    def chirp(m, f0, f1, am=0.0, amf=0.0, harm=0.25):
        tt = np.arange(m) / sr
        f = np.geomspace(f0, f1, m)
        ph = 2 * np.pi * np.cumsum(f) / sr
        e = np.sin(np.pi * np.arange(m) / m) ** 1.5
        a = 1 - am * (0.5 + 0.5 * np.sin(2 * np.pi * amf * tt))
        return (np.sin(ph) + harm * np.sin(2 * ph)) * e * a

    at = 0.12 * n                                   # a few lone chirps
    for _ in range(4):
        place(out, chirp(int(sr * 0.05), r.uniform(2800, 3600), r.uniform(4200, 5200)) * 0.5, at)
        at += sr * r.uniform(0.06, 0.12)
    trill = chirp(int(sr * 0.28), 3900, 3300, am=0.95, amf=34)    # a trill
    place(out, trill * 0.6, 0.42 * n)
    for j in range(5):                                         # a call: a falling phrase of whistles
        place(out, chirp(int(sr * 0.07), 4600 - j * 300, 3800 - j * 320, harm=0.1) * 0.55, 0.62 * n + j * sr * 0.075)
    for j in range(3):                                         # the answer, further away (lower, softer)
        c = chirp(int(sr * 0.09), 2400 + j * 200, 3100 + j * 150, am=0.6, amf=22)
        place(out, onepole_lp(c, 2500, sr) * 0.35, 0.86 * n + j * sr * 0.05)
    return out


def night(n, sr, r):
    t = np.arange(n) / sr
    out = 0.08 * np.sin(2 * np.pi * 60 * t) * (1 + 0.3 * np.sin(2 * np.pi * 0.7 * t))   # a far hum
    out += 0.15 * svf(pink(n, r), 180, 1.5, sr, "bp")
    for cf, rate, ph0, lvl in ((4400, 2.6, 0.0, 0.35), (4700, 3.1, 0.13, 0.25), (4100, 2.2, 0.31, 0.18)):
        # crickets: chirps of 3 pulses
        car = np.sin(2 * np.pi * cf * t)
        cyc = ((t * rate + ph0) % 1.0)
        pulse = np.where(cyc < 0.18, np.sin(np.pi * ((cyc / 0.06) % 1.0)) ** 2, 0.0)
        out += car * pulse * lvl
    fr = ramp(n, (0, 0.0), (0.35, 0.0), (0.45, 1.0), (1, 1.0))   # frogs, from a third on: a croak, its echo
    for at, f0 in ((0.4, 180), (0.52, 230), (0.68, 170), (0.78, 240), (0.9, 200)):
        m = int(sr * 0.16)
        tt = np.arange(m) / sr
        ph = 2 * np.pi * f0 * tt
        pulses = (np.sin(2 * np.pi * 28 * tt) > 0.2).astype(float)
        cro = svf(np.sign(np.sin(ph)) * pulses, 800, 0.5, sr, "bp") * np.sin(np.pi * tt / tt[-1]) * 0.5
        place(out, cro * fr[min(int(at * n), n - 1)], at * n)
    return out


# ---- DREAM

CHORD = [0, 7, 14, 16, 19, 24]                       # C G D E G C: open, a little bright


def glass(n, sr, r):
    t = np.arange(n) / sr
    out = np.zeros(n)
    for j, semi in enumerate(CHORD):                 # bells arriving one by one
        f = C4 * 2 ** (semi / 12) / 2
        at = int(j * n * 0.11)
        m = n - at
        tt = t[:m]
        idx = 2.2 * np.exp(-tt * 3)                   # FM: bright attack, glassy tail
        mod = np.sin(2 * np.pi * f * 3.5 * tt)
        b = np.sin(2 * np.pi * f * tt + idx * mod) * np.exp(-tt * 1.6)
        b += 0.3 * np.sin(2 * np.pi * f * 2.01 * tt) * np.exp(-tt * 2.5)
        place(out, b * 0.4, at)
    sh = np.sin(2 * np.pi * C4 * 4 * t + 0.5 * np.sin(2 * np.pi * 5.5 * t))   # the shimmer: two octaves up, beating
    sh += np.sin(2 * np.pi * C4 * 4.007 * t)
    out += 0.08 * sh * ramp(n, (0, 0), (0.5, 0.6), (1, 1))
    return out


def drift(n, sr, r):
    t = np.arange(n) / sr
    x = np.zeros(n)
    for semi in (0, 7, 12, 16):
        f = C4 * 2 ** (semi / 12) / 2
        for d in (-0.13, -0.05, 0.04, 0.12):          # detuned saws
            x += blsaw(f * 2 ** (d / 12), t, sr, r.uniform(0, 6.28))
    fc = ramp(n, (0, 300), (0.45, 4200), (0.6, 3600), (1, 350)) * (1 + 0.15 * np.sin(2 * np.pi * 1.3 * t))
    return svf(x / 16, fc, 0.6, sr, "lp")


def vox(n, sr, r):
    t = np.arange(n) / sr
    vib = ramp(n, (0, 0.0), (1, 0.35)) * np.sin(2 * np.pi * 5.2 * t)
    f0 = C4 / 2 * 2 ** (vib / 12)
    ph = np.cumsum(f0) / sr
    glot = (ph % 1.0)
    src = np.where(glot < 0.6, np.sin(np.pi * glot / 0.6) ** 2, 0.0)       # a soft glottal pulse
    src = np.diff(src, prepend=0) * 40 + 0.02 * r.standard_normal(n)
    V = {"a": (800, 1150, 2800), "e": (400, 1600, 2700), "i": (300, 2200, 3000), "o": (450, 800, 2830),
         "u": (325, 700, 2530)}
    seq = ["a", "e", "i", "o", "u"]
    out = np.zeros(n)
    for fi in range(3):
        fc = np.interp(np.linspace(0, len(seq) - 1, n), range(len(seq)), [V[v][fi] for v in seq])
        out += svf(src, fc, 0.12, sr, "bp") * (1.0, 0.6, 0.25)[fi]
    return out


def liquid(n, sr, r):
    t = np.arange(n) / sr
    f = C4 / 4
    x = blsaw(f, t, sr) + 0.5 * blsaw(f * 1.005, t, sr, 1.0)
    wob = np.sin(2 * np.pi * np.cumsum(ramp(n, (0, 1.5), (1, 9.0))) / sr)       # the wobble, speeding up
    fc = 250 + 2600 * (0.5 + 0.5 * wob) ** 2 * ramp(n, (0, 0.5), (1, 1.0))
    y = soft(svf(x, fc, 0.18, sr, "lp") * 0.5, 2.0)
    ring = np.sin(2 * np.pi * C4 * 3 * t) * np.sin(2 * np.pi * 1663 * t) * ramp(n, (0, 0), (0.6, 0.15), (1, 0.3))
    return y + ring


# ---- GLITCH

def data(n, sr, r):
    out = np.zeros(n)
    k = 0
    while k < n:
        m = int(sr * r.uniform(0.015, 0.09))
        tt = np.arange(m) / sr
        kind = r.integers(0, 6)
        if kind == 0:                                     # crushed noise
            hold = r.integers(4, 40)
            s = np.repeat(r.uniform(-1, 1, m // hold + 1), hold)[:m]
            s = np.round(s * 3) / 3
        elif kind == 1:                                   # a square bleep
            f = C4 * 2 ** (r.choice([0, 7, 12, 19, 24, 31]) / 12)
            s = np.sign(np.sin(2 * np.pi * f * tt)) * 0.6
        elif kind == 2:                                   # sample-and-hold tones
            steps = r.uniform(300, 3000, 8)
            f = np.repeat(steps, m // 8 + 1)[:m]
            s = np.sign(np.sin(2 * np.pi * np.cumsum(f) / sr)) * 0.5
        elif kind == 3:                                   # a digital chirp down
            f = np.geomspace(r.uniform(3000, 7000), r.uniform(80, 300), m)
            s = np.sin(2 * np.pi * np.cumsum(f) / sr)
        elif kind == 4:                                   # silence: a gap
            s = np.zeros(m)
        else:                                             # bit-shifted buzz
            s = (((np.arange(m) * r.integers(3, 17)) >> r.integers(2, 6)) & 15) / 7.5 - 1
        out[k:k + m] = s[:max(0, min(m, n - k))]
        k += m
    return out * 0.8


def circuit(n, sr, r):
    out = np.zeros(n)
    seg = int(sr * 0.06)
    x = 0.4
    y = 0.0
    phm = 0.0
    phc = 0.0
    fb = 0.0
    for s0 in range(0, n, seg):                          # parameters stepping every 60 ms: a bent circuit's knobs
        rr = r.uniform(3.6, 3.99)                           # the logistic map's rate: chaos
        fc = C4 * 2 ** (r.integers(-12, 25) / 12)
        ratio = r.choice([0.5, 1.0, 1.41, 2.0, 3.17, 5.0])
        beta = r.uniform(0.3, 1.6)
        mix = r.uniform(0.2, 0.8)
        for i in range(s0, min(n, s0 + seg)):
            if i % 7 == 0:
                x = rr * x * (1 - x)
            phm += 2 * np.pi * fc * ratio / sr
            phc += 2 * np.pi * fc * (1 + 0.5 * (x - 0.5)) / sr
            fb = np.sin(phm + beta * fb)                    # feedback FM
            y = np.sin(phc + 2.5 * fb)
            out[i] = mix * y + (1 - mix) * (x * 2 - 1) * 0.5
    return out


def tape(n, sr, r):
    t = np.arange(n) / sr
    notes = [0, 7, 12, 16, 19, 16, 12, 7]               # an arpeggio
    step = n // len(notes)
    f = np.concatenate([np.full(step, C4 * 2 ** (s / 12)) for s in notes] + [np.full(n - step * len(notes), C4)])
    wow = 1 + 0.012 * np.sin(2 * np.pi * 0.9 * t) + 0.004 * np.sin(2 * np.pi * 11 * t)
    speed = ramp(n, (0, 1), (0.55, 1), (0.6, 0.6), (0.7, 1.15), (0.78, 1), (1, 0.5))   # a jam, a lurch, a slowdown
    ph = 2 * np.pi * np.cumsum(f * wow * speed) / sr
    x = soft(0.7 * np.sin(ph) + 0.3 * np.sin(2 * ph) + 0.15 * saw(ph), 2.2)
    drop = np.ones(n)
    for at in (0.3, 0.47, 0.82):                         # drop-outs
        a, b = int(at * n), int((at + r.uniform(0.02, 0.05)) * n)
        drop[a:b] = 0.08
    drop = onepole_lp(drop, 80, sr)
    hiss = onepole_lp(r.standard_normal(n), 6000, sr) * 0.04
    return onepole_lp(x * drop, 5000, sr) + hiss


def static(n, sr, r):
    t = np.arange(n) / sr
    hiss = svf(r.standard_normal(n), ramp(n, (0, 2000), (1, 5000)), 0.9, sr, "bp") * 0.4
    whis = np.sin(2 * np.pi * np.cumsum(ramp(n, (0, 2600), (0.3, 400), (0.5, 1800), (1, 900))) / sr)
    whis *= ramp(n, (0, 0.5), (0.3, 0.2), (0.5, 0.4), (0.6, 0.0), (1, 0.0))
    # a formant voice through the noise (no words: a buzz through vowel filters, AM'd like a syllable rhythm)
    buzz = saw(2 * np.pi * 120 * t * (1 + 0.05 * np.sin(2 * np.pi * 3 * t)))
    syl = (0.5 + 0.5 * np.sin(2 * np.pi * 4.5 * t)) ** 2
    voice = (svf(buzz, 700 + 400 * np.sin(2 * np.pi * 2.2 * t), 0.2, sr, "bp")
             + 0.5 * svf(buzz, 1800 + 500 * np.sin(2 * np.pi * 1.7 * t), 0.2, sr, "bp")) * syl
    voice *= ramp(n, (0, 0), (0.55, 0), (0.62, 0.5), (0.85, 0.5), (0.9, 0))
    morse = np.zeros(n)                                  # morse beeps at the end
    k = int(0.86 * n)
    for d in (1, 1, 3, 1, 3, 3, 1):
        m = int(sr * 0.025 * d)
        morse[k:k + m] = np.sin(2 * np.pi * 1200 * t[:m][:max(0, n - k)])[:max(0, min(m, n - k))]
        k += m + int(sr * 0.025)
    return hiss * ramp(n, (0, 1), (0.6, 0.7), (1, 0.5)) + whis * 0.5 + voice * 0.6 + morse * 0.5


GEN = {"RAIN": rain, "CREEK": creek, "BIRDS": birds, "NIGHT": night, "GLASS": glass, "DRIFT": drift, "VOX": vox,
       "LIQUID": liquid, "DATA": data, "CIRCUIT": circuit, "TAPE": tape, "STATIC": static}


def make(name, sr, sec):
    n = int(sr * sec)
    x = np.asarray(GEN[name](n, sr, rng(name)), dtype=float)[:n]
    x -= onepole_lp(x, 35, sr)                           # no DC, not even a part's (a grain hears only its part)
    x = onepole_lp(x, sr * 0.45, sr)                     # nothing at the Nyquist
    x /= max(1e-9, 4 * np.sqrt(np.mean(x ** 2)))         # the same loudness each (RMS 1/4), the peaks rounded off
    x = np.tanh(x * 1.2) / np.tanh(1.2) * 0.98
    for _ in range(2):                                   # (the rounding is not symmetric: its DC off again)
        x -= onepole_lp(x, 40, sr)
    x /= max(1.0, np.max(np.abs(x)) / 0.98)
    fade = int(sr * 0.012)
    w = np.sin(np.linspace(0, np.pi / 2, fade)) ** 2
    x[:fade] *= w
    x[-fade:] *= w[::-1]
    return (x * 30000).astype(np.int16)


def main(outdir):
    out = Path(outdir)
    for setname, sr, items in SETS:
        d = out / setname
        d.mkdir(parents=True, exist_ok=True)
        for k, (name, sec) in enumerate(items):
            x = make(name, sr, sec)
            with wave.open(str(d / f"{k} {name}.wav"), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(sr)
                w.writeframes(x.tobytes())
    total = sum(int(sr * sec) for _, sr, items in SETS for _, sec in items)
    print(f"canvas material: {sum(len(i) for _, _, i in SETS)} sounds, {total} samples, ~{total // 2} B as ADPCM")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build/canvaswav")
