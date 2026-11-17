#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Felucca Canvas (fork of Felucca, Copyright (C) 2026 Leo Kuroshita, Hügelton Instruments)
"""The Felucca Canvas site (GitHub Pages): a page listing the firmwares, each with screenshots and its own installer.

  tools/make_canvas_site.py OUT_DIR KEY=PACKAGE.fwsc [KEY=PACKAGE.fwsc ...]

KEY is one of the FIRMWARES below (16fx, 27, 8ctl). Writes
  OUT_DIR/index.html                      the list (thumbnails: docs/img/canvas-KEY*.png)
  OUT_DIR/img/                            the screenshots
  OUT_DIR/KEY/webapp/installer/           that firmware's installer (web/make_site.py; its progress bar, a link back)
  OUT_DIR/KEY/firmware/felucca-VER.fwsc   the package
The version shown is read from the package's file name (felucca-VER.fwsc).
"""
import html
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "web"))
import make_site  # noqa: E402

FIRMWARES = {
    "16fx": ("Canvas 16+FX",
             "16 grains on the white keys, 11 glitches on the black keys (beat repeat, reverse, freeze, tape stop, "
             "scatter, pitch jumps, burst, crush, octave up / down). The all-round performance layout."),
    "27": ("Canvas 27",
           "Every one of the 27 keys is its own grain. Glitches come from the FX layer: hold FX and press a white key."),
    "8ctl": ("Canvas 8+CTL",
             "8 grains, the 11 glitches, and 8 performance controls: all off, randomize, scan, smaller / larger grains, "
             "pitch down / up, latch."),
}

PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Felucca Canvas</title>
<style>
  :root {{ --bg: #05010d; --fg: #f3eaff; --dim: #b49ad8; --pink: #ff2bd6; --cyan: #19f0ff; --lime: #8dff5a; }}
  html {{ background: var(--bg); }}
  body {{ margin: 0; color: var(--fg); font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }}
  #bg {{ position: fixed; inset: 0; width: 100vw; height: 100vh; z-index: -2; filter: saturate(1.25); }}
  .veil {{ position: fixed; inset: 0; z-index: -1;
          background: radial-gradient(ellipse at 50% 40%, rgba(5,1,13,.15), rgba(5,1,13,.7) 80%); }}
  main {{ max-width: 1000px; margin: 0 auto; padding: 56px 16px; }}
  h1 {{ font-size: 40px; font-weight: 800; margin: 0 0 8px; letter-spacing: .02em;
        background: linear-gradient(90deg, var(--pink), var(--cyan) 55%, var(--lime));
        -webkit-background-clip: text; background-clip: text; color: transparent;
        filter: drop-shadow(0 0 10px rgba(255,43,214,.55)) drop-shadow(0 0 22px rgba(25,240,255,.35)); }}
  .lead {{ color: var(--fg); opacity: .85; margin: 0 0 36px; max-width: 760px; text-shadow: 0 1px 8px #05010d, 0 0 2px #05010d; }}
  .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 22px; }}
  .card {{ background: rgba(12,2,28,.72); backdrop-filter: blur(6px); -webkit-backdrop-filter: blur(6px);
          border: 1px solid rgba(25,240,255,.45); border-radius: 14px; padding: 16px; display: flex; flex-direction: column;
          box-shadow: 0 0 14px rgba(25,240,255,.25), inset 0 0 18px rgba(255,43,214,.12);
          transition: box-shadow .2s, border-color .2s, transform .2s; }}
  .card:hover {{ border-color: var(--pink); transform: translateY(-2px);
                box-shadow: 0 0 22px rgba(255,43,214,.55), inset 0 0 22px rgba(25,240,255,.18); }}
  .thumbs {{ display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 12px; }}
  .thumbs img {{ width: 100%; border-radius: 6px; image-rendering: pixelated; display: block;
                border: 1px solid rgba(255,43,214,.5); box-shadow: 0 0 10px rgba(255,43,214,.35); }}
  h2 {{ font-size: 20px; margin: 0; color: var(--cyan); text-shadow: 0 0 8px rgba(25,240,255,.8); }}
  .ver {{ color: var(--lime); font-size: 13px; margin: 0 0 8px; text-shadow: 0 0 6px rgba(141,255,90,.6); }}
  .card p {{ margin: 0 0 16px; font-size: 14px; }}
  .card p.desc {{ flex: 1; }}
  a.btn {{ display: block; text-align: center; color: #0a0014; text-decoration: none; padding: 11px; border-radius: 10px;
          font-weight: 700; background: linear-gradient(90deg, var(--pink), var(--cyan));
          box-shadow: 0 0 14px rgba(255,43,214,.6), 0 0 26px rgba(25,240,255,.35); }}
  a.btn:hover {{ filter: brightness(1.15); }}
  .small {{ color: var(--dim); font-size: 13px; margin-top: 36px; }}
  .small a {{ color: var(--cyan); }}
</style></head>
<body>
<canvas id="bg"></canvas><div class="veil"></div>
<main>
  <h1>Felucca Canvas</h1>
  <p class="lead">Granular performance firmware for the M-VAVE FM-1: every grain key is its own stream of grains, black keys
  hold glitches. Pick a layout, connect the FM-1 by USB, and install from Chrome or Edge. Unofficial; install at your own
  risk and back up first.</p>
  <div class="grid">
{cards}
  </div>
  <p class="small">Built on <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (Hügelton Instruments),
  GPL-3.0, no warranty. Each installer also offers the way back to the official FM-1 firmware.
  M-VAVE and FM-1 are trademarks of their respective owners.</p>
</main>
<script>
{fractal}
neonFractal(document.getElementById("bg"), true);
</script>
</body></html>
"""

CARD = """    <div class="card">
      <div class="thumbs">
        <a href="img/canvas-{key}.png"><img src="img/canvas-{key}.png" alt="{name}: the GRAINS page"></a>
        <a href="img/canvas-{key}-glitch.png"><img src="img/canvas-{key}-glitch.png" alt="{name}: grains on, a glitch held"></a>
      </div>
      <h2>{name}</h2>
      <p class="ver">{version}</p>
      <p class="desc">{desc}</p>
      <a class="btn" href="{key}/webapp/installer/">Install {name}</a>
    </div>"""


def main(out, pairs):
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    (out / "img").mkdir(parents=True)
    cards = []
    for pair in pairs:
        key, pkg = pair.split("=", 1)
        if key not in FIRMWARES:
            raise SystemExit(f"{key}: not one of {', '.join(FIRMWARES)}")
        name, desc = FIRMWARES[key]
        m = re.fullmatch(r"felucca-(.+)\.fwsc", Path(pkg).name)
        version = m.group(1) if m else "dev"
        make_site.main(pkg, version, out / key, title=f"Felucca {name}", back="../../../")
        for suf in ("", "-glitch"):
            shutil.copy(ROOT / "docs" / "img" / f"canvas-{key}{suf}.png", out / "img")
        cards.append(CARD.format(key=key, name=html.escape(name), version=html.escape("v" + version),
                                 desc=html.escape(desc)))
    fractal = (ROOT / "web" / "neon_fractal.js").read_text(encoding="utf-8")
    (out / "index.html").write_text(PAGE.format(cards="\n".join(cards), fractal=fractal), encoding="utf-8")
    # The first Canvas site had a single installer at webapp/installer/ (Felucca 1.8-cnv). Old bookmarks and cached
    # tabs still open that address, so send them to the firmware list (and the editor to the 16+FX build's editor).
    for sub, to in (("webapp/installer", "../../"), ("webapp/editor", "../../16fx/webapp/editor/"), ("webapp", "../")):
        (out / sub).mkdir(parents=True, exist_ok=True)
        (out / sub / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>Felucca Canvas</title>'
            f'<meta http-equiv="refresh" content="0; url={to}">'
            '<meta http-equiv="cache-control" content="no-cache">'
            f'<p>This page moved. <a href="{to}">Felucca Canvas firmwares</a></p>', encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"canvas site: {out}: {len(cards)} firmwares")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2:])
