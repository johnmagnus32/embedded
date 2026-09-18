#!/usr/bin/env python3
"""Generate ORIGINAL synthesized sound effects (+ a short looping music bed) for canvas games as
44.1 kHz stereo 16-bit WAV, into src/assets/sfx/. Pure synthesis — oscillators (sine/square/tri) +
amplitude envelopes; no sampled or copyrighted audio, and the music is a plain arpeggio over a
generic chord progression (not any existing tune). Matches the engine mixer's format (RATE/stereo/
int16) so no resampling is needed at load. Run: python3 tools/gen_sfx.py
"""
import os, math, wave, struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "sfx")
os.makedirs(OUT, exist_ok=True)
RATE = 44100

def osc(freq, i, kind):
    p = (freq * i / RATE) % 1.0
    if kind == "sine":   return math.sin(2 * math.pi * p)
    if kind == "square": return 1.0 if p < 0.5 else -1.0
    if kind == "tri":    return 4 * abs(p - 0.5) - 1.0
    if kind == "saw":    return 2 * p - 1.0
    return 0.0

def render(dur, fn):
    n = int(dur * RATE); buf = bytearray()
    for i in range(n):
        s = max(-1.0, min(1.0, fn(i, n)))
        v = int(s * 30000)
        buf += struct.pack("<hh", v, v)       # stereo (dup L/R)
    return buf

def save(name, frames):
    w = wave.open(os.path.join(OUT, name), "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(bytes(frames)); w.close()
    print("wrote", os.path.join("assets/sfx", name), len(frames) // 4, "frames")

# --- SFX ---------------------------------------------------------------------------------
def f_pop(i, n):                               # short bright blip, quick downward chirp
    t = i / RATE; f = 760 - 300 * (t / (n / RATE))
    return osc(f, i, "sine") * math.exp(-t * 32) * 0.9

def f_leak(i, n):                              # descending "aww" — a balloon reached the end
    t = i / RATE; f = 520 - 300 * (t / (n / RATE))
    return (osc(f, i, "tri") * 0.6 + osc(f * 2, i, "sine") * 0.2) * min(1.0, t / 0.01) * math.exp(-t * 4)

def f_place(i, n):                             # soft low "thock" — a monkey is placed
    return osc(230, i, "sine") * math.exp(-(i / RATE) * 40)

def f_ui(i, n):                                # tiny tick — a button/UI tap
    return osc(1250, i, "square") * math.exp(-(i / RATE) * 80) * 0.5

def f_start(i, n):                             # quick ascending arpeggio — wave start
    t = i / RATE; notes = [523, 659, 784]; seg = 0.09
    k = min(2, int(t / seg)); lt = t - k * seg
    return osc(notes[k], i, "square") * math.exp(-lt * 14) * 0.5

def f_flap(i, n):                              # a quick upward "flap" chirp
    t = i / RATE; f = 320 + 520 * (t / (n / RATE))
    return osc(f, i, "square") * math.exp(-t * 26) * 0.6

def f_score(i, n):                             # bright two-note "point!" ding (B5 -> E6)
    t = i / RATE; seg = 0.07; k = min(1, int(t / seg)); lt = t - k * seg
    return osc([988, 1319][k], i, "square") * math.exp(-lt * 16) * 0.5

def f_hit(i, n):                               # low descending thud on death
    t = i / RATE; f = 200 - 130 * (t / (n / RATE))
    return (osc(f, i, "tri") * 0.7 + osc(f * 1.5, i, "sine") * 0.2) * math.exp(-t * 10)

save("flap.wav",  render(0.09, f_flap))
save("score.wav", render(0.16, f_score))
save("hit.wav",   render(0.28, f_hit))
save("pop.wav",   render(0.12, f_pop))
save("leak.wav",  render(0.38, f_leak))
save("place.wav", render(0.10, f_place))
save("ui.wav",    render(0.05, f_ui))
save("start.wav", render(0.30, f_start))

# --- music: a gentle looping arpeggio, 4 bars x 1s (original) -----------------------------
CHORDS = [[220, 262, 330], [175, 220, 262], [262, 330, 392], [196, 247, 294]]  # Am F C G-ish
def f_music(i, n):
    t = i / RATE; bar = int(t) % 4; ch = CHORDS[bar]
    pluck = 0.125; k = int((t % 1.0) / pluck); lt = t % pluck; f = ch[k % 3]
    return (osc(f, i, "tri") * 0.5 + osc(f * 2, i, "sine") * 0.15) * math.exp(-lt * 6) * 0.30

save("music.wav", render(4.0, f_music))

# --- a driving 120 BPM loop for the rhythm game (clear kick pulse so notes line up) ---
BASS = [110, 110, 146, 110, 98, 98, 130, 110]
def f_rhythm(i, n):
    t = i / RATE; beat = 0.5; tb = t % beat; th = t % (beat / 2)
    kick = osc(58, i, "sine") * math.exp(-tb * 22) * 0.9 if tb < 0.2 else 0.0
    hat  = osc(9000, i, "square") * math.exp(-th * 90) * 0.22
    bass = osc(BASS[int(t / beat) % len(BASS)], i, "tri") * math.exp(-tb * 3) * 0.30
    return kick + hat + bass
save("rhythm.wav", render(8.0, f_rhythm))

# --- doom-lite: shotgun blast, imp death growl, player hurt (deterministic pseudo-noise) --------
def noise(i):                                  # cheap repeatable white-ish noise in [-1,1]
    x = math.sin(i * 12.9898) * 43758.5453
    return 2.0 * (x - math.floor(x)) - 1.0

def f_shotgun(i, n):                           # noisy crack + low thump, fast decay
    t = i / RATE
    crack = noise(i) * math.exp(-t * 34) * 0.9
    thump = osc(90, i, "sine") * math.exp(-t * 20) * 0.5
    return crack + thump

def f_growl(i, n):                             # descending guttural growl on imp death
    t = i / RATE; f = 240 - 150 * (t / (n / RATE))
    return (osc(f, i, "saw") * 0.55 + noise(i) * 0.15) * math.exp(-t * 7)

def f_hurt(i, n):                              # short mid grunt when the player is hit
    t = i / RATE
    return (osc(300, i, "square") * 0.5 + noise(i) * 0.2) * math.exp(-t * 22)

save("shotgun.wav", render(0.24, f_shotgun))
save("growl.wav",   render(0.34, f_growl))
save("hurt.wav",    render(0.12, f_hurt))

# --- nova (3D shooter): laser pew + explosion boom -----------------------------------------
def f_laser(i, n):                             # fast downward zap
    t = i / RATE; f = 1400 - 1050 * (t / (n / RATE))
    return (osc(f, i, "square") * 0.5 + osc(f * 1.5, i, "sine") * 0.2) * math.exp(-t * 26)

def f_boom(i, n):                              # noisy explosion + low body
    t = i / RATE
    return (noise(i) * math.exp(-t * 11) * 0.8 + osc(70, i, "sine") * math.exp(-t * 9) * 0.5)

save("laser.wav", render(0.14, f_laser))
save("boom.wav",  render(0.40, f_boom))

# --- circuit (3D racer): a looping engine drone (buzzy saw + harmonics) --------------------
def f_engine(i, n):
    t = i / RATE
    return (osc(72, i, "saw") * 0.34 + osc(144, i, "saw") * 0.16 + osc(216, i, "square") * 0.08
            + noise(i) * 0.04)
save("engine.wav", render(1.0, f_engine))

# --- kart: upbeat ORIGINAL music loop (drums+bass+chord stabs+lead; I-V-vi-IV in C, 150 BPM) ---
def f_kartmusic(i,n):
    t=i/RATE; bar=int(t/1.6)%4; tb=t%0.4; bi=int(t/0.4); e8=t%0.2
    roots=[65.41,98.0,110.0,87.31]
    TR=[[261.6,329.6,392.0],[196.0,246.9,293.7],[220.0,261.6,329.6],[174.6,220.0,261.6]][bar]
    root=roots[bar]
    kick=osc(55,i,"sine")*math.exp(-tb*28)*0.95 if tb<0.17 else 0.0
    snare=noise(i)*0.5*math.exp(-tb*22) if (bi%2==1 and tb<0.13) else 0.0
    hat=noise(i)*math.exp(-e8*90)*0.12
    bass=osc(root,i,"saw")*math.exp(-tb*2.5)*0.26
    stab=0.0
    if e8<0.09:
        for fr in TR: stab+=osc(fr,i,"square")
        stab*=math.exp(-e8*12)*0.05
    step=int(t/0.2)%3; lead=osc(TR[step]*2,i,"tri")*math.exp(-e8*7)*0.16
    return (kick+snare+hat+bass+stab+lead)*0.72
save("kartmusic.wav", render(6.4, f_kartmusic))
def f_kboost(i,n):
    t=i/RATE; T=n/RATE
    return osc(170+1350*(t/T),i,"saw")*math.exp(-t*4)*0.4 + noise(i)*math.exp(-t*7)*0.3
save("kboost.wav", render(0.5, f_kboost))
def f_ding(i,n):
    t=i/RATE; notes=[523,659,784,1047]; seg=0.06; k=min(3,int(t/seg)); lt=t-k*seg
    return osc(notes[k],i,"square")*math.exp(-lt*18)*0.5
save("ding.wav", render(0.26, f_ding))
def f_bump(i,n):
    t=i/RATE
    return osc(85,i,"sine")*math.exp(-t*14)*0.8 + noise(i)*math.exp(-t*30)*0.4
save("bump.wav", render(0.2, f_bump))
