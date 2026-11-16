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
  :root {{ --bg: #000; --fg: #fff; --dim: #888; --card: #141414; }}
  body {{ margin: 0; background: var(--bg); color: var(--fg); font: 16px/1.6 -apple-system, BlinkMacSystemFont,
         "Helvetica Neue", Arial, sans-serif; }}
  main {{ max-width: 980px; margin: 0 auto; padding: 48px 16px; }}
  h1 {{ font-size: 28px; font-weight: 600; margin: 0 0 8px; }}
  .lead {{ color: var(--dim); margin: 0 0 32px; }}
  .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; }}
  .card {{ background: var(--card); border-radius: 12px; padding: 16px; display: flex; flex-direction: column; }}
  .thumbs {{ display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 12px; }}
  .thumbs img {{ width: 100%; border-radius: 6px; image-rendering: pixelated; display: block; }}
  h2 {{ font-size: 19px; margin: 0; }}
  .ver {{ color: var(--dim); font-size: 13px; margin: 0 0 8px; }}
  .card p {{ margin: 0 0 16px; font-size: 14px; }}
  .card p.desc {{ flex: 1; }}
  a.btn {{ display: block; text-align: center; background: var(--fg); color: var(--bg); text-decoration: none;
          padding: 10px; border-radius: 8px; font-weight: 600; }}
  .small {{ color: var(--dim); font-size: 13px; margin-top: 32px; }}
  .small a {{ color: inherit; }}
</style></head>
<body><main>
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
</main></body></html>
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
    (out / "index.html").write_text(PAGE.format(cards="\n".join(cards)), encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"canvas site: {out}: {len(cards)} firmwares")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2:])
