# Felucca Canvas: a granular performance instrument for the M-VAVE FM-1

This fork of [Felucca](https://github.com/hugelton/Felucca) adds **CANVAS**, a granular engine made for playing live, in
the spirit of a hardware granular synth. **The keys are grains**: each grain key runs its own stream of grains, with its own
position in the sample material, grain size, density and pitch. Tap keys to start streams and let them sustain; they blend
like paint. Other keys hold **glitch effects**.

**The three firmwares on the install page are granular only.** All four tracks are CANVAS, PRESETS offers only the
granular sounds, and HOME always brings back the GRAINS page with its fractal (tap HOME again for the MIXER). A project or
user preset made with another engine plays its track as CANVAS. (A build without `FELUCCA_CANVAS_KEYS` keeps every
Felucca engine, with CANVAS as one of them.)

### The sounds

The granular firmwares carry their own source material, synthesized by `tools/gen_canvas_material.py` (no recordings,
nothing third-party). SRC picks a set; POS runs through its four sounds, a quarter each, and every sound changes along its
length, so POS, SCAN, SPRY and the LFO move through it:

| SRC | POS 0..31 | 32..63 | 64..95 | 96..127 |
|---|---|---|---|---|
| NATURE | RAIN (downpour to last drops) | CREEK (bubbles over a rush) | BIRDS (wind, chirps, a trill, a call and answer) | NIGHT (crickets, frogs, hum) |
| DREAM | GLASS (FM bells arriving) | DRIFT (wide chord, filter opening and closing) | VOX (a sung a-e-i-o-u) | LIQUID (resonant wobble speeding up) |
| GLITCH | DATA (crushed bursts, bleeps) | CIRCUIT (chaotic feedback FM) | TAPE (a failing tape arpeggio) | STATIC (radio tuning, morse) |

MIX spreads eight of the twelve across POS. The 17 factory sounds each set every grain differently (place, size, density,
a pitch from a scale) and add movement with the LFO and an INSERT:
RAINFALL, CREEK, DAWN SONG, NIGHT POND, WILDWOOD (NATURE); GLASS GDN, SLOW BLOOM, CHOIR, MELT, DREAMSCAPE (DREAM);
DATA RAIN, BENT, WOW+FLUTR, SHORTWAVE, MELTDOWN (GLITCH); STORM, BIOLUMIN (MIX). The four tracks start as RAINFALL,
GLASS GDN, DATA RAIN and STORM.

> Unofficial firmware. Not affiliated with M-VAVE, Hügelton Instruments or any granular-synth maker. You install it at
> your own risk; back up your projects and presets first (the web editor's backup).

## Getting started

1. The granular firmwares start on the GRAINS page, every track CANVAS; turn PRESETS on any other page for another sound.
   (In a build of every engine: on a track, hold **EDIT** and press **A4** for CANVAS, or turn PRESETS to a CANVAS sound.)
2. **Tap a white key**: a grain stream starts and keeps sounding. Tap more keys to layer them; tap one again to fade it out.
3. **Hold a black key** for a glitch while it's held.
4. **Hold a grain key and turn KNOB 1..4** to change that grain's position, size, density and pitch while it plays.

## The key layouts (KEYS, EDIT 1)

| KEYS | White keys | Black keys |
| --- | --- | --- |
| **16+FX** (default) | 16 grains (F3 .. G5) | 11 glitches |
| **27** | grains 1..16 | grains 17..27 (every key a grain) |
| **8+CTL** | F3..F4: 8 grains; G4..G5: 8 controls | 11 glitches |

With **27**, glitches come from Felucca's FX layer: hold **FX** and press a white key (beat repeat, reverse, tape stop,
freeze, octave up/down, filters, flanger, phaser, throws).

**Glitches** (black keys, left to right, F#3 .. F#5):

| Key | Glitch | |
| --- | --- | --- |
| F#3 | RPT16 | Beat repeat 1/16 (FX layer, whole mix) |
| G#3 | RPT32 | Beat repeat 1/32 (FX layer, whole mix) |
| A#3 | REVRS | Reverse (FX layer, whole mix) |
| C#4 | FREEZ | Freeze (FX layer, whole mix) |
| D#4 | TAPE | Tape stop (FX layer, whole mix) |
| F#4 | SCATR | Every grain jumps to a random place in the material |
| G#4 | JUMP | Every grain leaps an octave or a fifth up or down |
| A#4 | BURST | Four times the grains, a quarter of the size |
| C#5 | CRUSH | Bit and sample-rate crush on this track (depth: CRSH) |
| D#5 | OCT+ | Octave-up harmonizer (FX layer, whole mix) |
| F#5 | OCT- | Octave-down harmonizer (FX layer, whole mix) |

**Controls** (8+CTL, G4..G5): **ALL OFF** (every grain off), **RANDOM** (the sounding grains jump to new places),
**SCAN** (while held, every grain's position drifts through the material and stays where it got to), **SMALL** /
**LARGE** (while held, grains a quarter / three times the size), **PIT-** / **PIT+** (while held, an octave down / up),
**LATCH** (switches LATCH and HOLD).

## LATCH and HOLD (MODE, EDIT 1)

- **LATCH** (default): tap a grain key to start it, tap again to stop it. Several keep sounding at once.
- **HOLD**: a grain sounds while its key is held.

Starting and stopping fade over **FADE** (3 ms to about 1 s), so long fades crossfade textures smoothly.

## The GRAINS page (EDIT, tap again after EDIT 2)

A map of the keys, laid out like the FM-1: grains that are on are lit, grains fading out are grey, glitches and controls
light while held, and the picked grain is ringed. Under it: the picked grain (or the glitch or control pressed last),
and SRC.

| Control | Does |
| --- | --- |
| KNOB 1 POS | The picked grain's place in the material (0..127, across all of SRC's samples in turn) |
| KNOB 2 SIZE | Its grain length (10 ms .. ~0.66 s) |
| KNOB 3 DENS | Its grains a second (1 .. ~98) |
| KNOB 4 PTCH | Its pitch (±24 semitones) |
| PRESETS | Picks the next / previous grain without playing it |
| Keys | Play, as everywhere; pressing a grain key picks it |

The keys' lights show which grains are on, on every page.

## The other parameters

| | | |
| --- | --- | --- |
| SRC | MIX, PIANO, FLUTE, SAX, USR1..3 | The material. MIX: eight different built-in samples. USR1..3: your own, uploaded with the web editor (up to 16 samples a slot); an empty slot is silent |
| SPRY | 0..127 | Random spread of each grain's place and timing |
| RAND | 0..127 | Random detune per grain (up to ±1 octave) |
| TONE | 0..127 | Low-pass (127 open) |
| CRSH | 0..127 | How hard the CRUSH glitch crushes |

HOME's knobs: SPRY, FADE, TONE, SRC. **Modulation**: LFO → SHP moves every grain's place (slow LFO = moving
playhead), LFO → PIT their pitch, LFO / ENV → FLT the tone.

The **sequencer, arp and MIDI IN** play the keys too: note 53 + key (F3 = 53 .. G5 = 79; other notes fold into that
range). In HOLD mode a step holds a grain for its length; in LATCH mode a step toggles it. Glitches can be sequenced.

Up to 24 grains sound per track at once; past that, and past the decode-cost cap, the oldest keep playing and new ones wait.

## What is saved

Each track's 27 grains (position, size, density, pitch) are part of the sound:

- **Projects and the autosave**: stored in the track's FM6-patch area (a CANVAS track has no FM6 patch; on load its FM6
  patch resets to the init voice, which only matters if you later switch that track to FM6).
- **User presets** (SAVE > USER), and **undo** after a sound load.
- **Factory presets** each have their own set of grains.

Which grains are on (latched) is a performance state and isn't saved. FAV doesn't stick on CANVAS's factory presets
(Felucca's favourites table is full); star a user preset instead. The web editor sees the eight parameters but not the
27 grains (full backups include them).

## The three firmwares

The site lists three builds of the same source, one per key layout. Each has its layout fixed (KEYS can't be changed),
every factory preset in that layout, and track 1 powering on as CANVAS:

| Firmware | Version on the device | Build |
| --- | --- | --- |
| Canvas 16+FX | v1.8-c16 | `FELUCCA_CANVAS_KEYS=0 ./build.sh --release 1.8-c16` |
| Canvas 27 | v1.8-c27 | `FELUCCA_CANVAS_KEYS=1 ./build.sh --release 1.8-c27` |
| Canvas 8+CTL | v1.8-c8 | `FELUCCA_CANVAS_KEYS=2 ./build.sh --release 1.8-c8` |

A plain `./build.sh` builds the switchable one (KEYS picks the layout; track 1 powers on as upstream's ACID).

## Installing from a browser

The repository's GitHub Actions workflow (`.github/workflows/pages.yml`) builds the three firmwares in the cloud and
publishes the site on GitHub Pages: a page listing them with screenshots (`docs/img/`), and an installer for each with
a progress bar (the step-by-step log folded under Details) and a link back to the list.

1. Repository **Settings > Pages > Source: GitHub Actions** (once).
2. Every push to `main` (or **Actions > Build and publish installer > Run workflow**) builds and publishes. The three
   `.fwsc` files are also attached to each run.
3. Open https://mojotechnical.github.io/felucca-canvas/ (forks: `https://<your-user>.github.io/<repo>/`) in **Chrome or Edge**, pick a firmware, connect the FM-1 by USB and
   press Install.

The site generator is `tools/make_canvas_site.py` (it uses Felucca's `web/make_site.py` for each installer).

## Building and flashing

Building works exactly as upstream Felucca; see [BUILDING.md](../BUILDING.md). In short, on Linux x86-64:

```sh
git clone --recurse-submodules https://github.com/hugelton/Felucca.git
cd Felucca
git checkout ca1db43 && git submodule update    # the upstream commit the patch is made against
git apply /path/to/felucca-canvas.patch        # or skip these three steps and unpack the full source tarball
pip3 install Pillow fonttools
tools/get_toolchain.sh                           # JieLi's clang for the pi32v2 CPU
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
./build.sh                                       # -> build/felucca.fwsc
tests/run_tests.sh                               # optional: the host test suite, CANVAS included
```

On macOS, install Docker Desktop first; the toolchain runs in a `linux/amd64` container (see BUILDING.md).

To tell the build apart on the device, make a release build with a suffix: `./build.sh --release 1.8-cnv`. The menu
then shows `v1.8-cnv`.

Flash `build/felucca.fwsc` with Felucca's installer. The hosted web installer installs only upstream's release, so
use one of these:

- **Command line** (USB; `pip3 install mido python-rtmidi`):
  `python3 tools/fm1_install.py build/felucca.fwsc`
- **A local copy of the web installer** (Chrome or Edge; Web MIDI needs `localhost`):

  ```sh
  python3 web/make_site.py build/felucca.fwsc dev /tmp/felucca-site
  cd /tmp/felucca-site && python3 -m http.server 8000
  # open http://localhost:8000/webapp/installer/
  ```

If an install fails and the FM-1 no longer starts, recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) (see BUILDING.md).

To go back, install an upstream Felucca release with the hosted web installer, or use its "Return to official
V15/V16". A project
saved with a CANVAS track uses engine 15, which upstream Felucca doesn't know, so upstream refuses to load that
project rather than playing it wrongly. Projects without CANVAS tracks load anywhere.

## Engine numbers

CANVAS is engine **15** (`ENGI_CANVAS`). Engine 14 stays reserved for upstream's optional ADPCM engine; in builds
without ADPCM it's a silent placeholder that's never offered. `FELUCCA_CANVAS=0 ./build.sh` builds the firmware
without CANVAS, identical in behaviour to upstream.

## Source map

| File | What |
| --- | --- |
| `firmware/src/eng_canvas.c` | The engine: key layouts, grain streams, glitches, controls, presets, saving the grains |
| `firmware/src/ui_canvas.c` | The GRAINS page: picking, knobs, LEDs |
| `firmware/src/ui_graph.c` `graph_canvas` | The key map drawing |
| `firmware/src/ui_draw.c` | The page's four cards and its footer |
| `firmware/src/project.c`, `up_fm6.c`, `upreset.c`, `ui.c` | Saving the grains: projects, user presets, undo, preset loads |
| `firmware/src/perform.c` (unchanged) | The FX layer's master glitches, pressed by the black keys |
| `tests/canvas_test.c` | Engine tests and demo renders (`build/canvas_demo/`) |
| `tests/ui_test.c` `test_canvas` | Page, knobs, LEDs, persistence and undo tests |
| `tools/make_canvas_site.py`, `web/index_pkg.html` | The site: the firmware list and the installers (progress bar) |
