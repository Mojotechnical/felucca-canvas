# Felucca Canvas

## ▶ Install Felucca Canvas: https://mojotechnical.github.io/felucca-canvas/

**Use the link above, not the "Felucca" links further down.** Those go to upstream Felucca, the plain synth firmware without
CANVAS. The Felucca Canvas page lists the three builds (16+FX, 27, 8+CTL), each with its own Install button. The startup screen
shows which one is running: **v1.8-c16**, **v1.8-c27** or **v1.8-c8**.

Felucca Canvas is an unofficial fork of Felucca for the M-VAVE FM-1. It adds **CANVAS**, a granular performance engine: every key
is a grain stream with its own position, size, density and pitch, latched or held, blending as they sound; black keys hold glitch
effects, and the GRAINS page draws a fractal that reacts to what you play. See [docs/CANVAS.md](docs/CANVAS.md).

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

---

*Everything below is upstream Felucca's README. Its installer, editor and support links are upstream's, not this fork's.*

## Project resources

- [Upstream Felucca web installer](https://hugelton.github.io/Felucca/) (installs plain Felucca, not Canvas)
- [Web editor](https://hugelton.github.io/Felucca/webapp/editor/) ([source](https://github.com/hugelton/Felucca-WebApp))
- [Try it in your browser](https://hugelton.github.io/Felucca/webapp/try/): a demo of the firmware, no FM-1 needed
- [Manual and release notes](https://github.com/hugelton/Felucca/wiki/Home)
- [Building from source](BUILDING.md)
- [Issue tracker](https://github.com/hugelton/Felucca/issues)

The firmware uses [Felucca-Engines](https://github.com/hugelton/Felucca-Engines) (MIT) for its built-in sound engines and [felucca-bsp](https://github.com/hugelton/felucca-bsp) (MPL-2.0) for the FM-1 hardware layer. Neither repository implies support for Felucca on other hardware.

## Development and support

Felucca focuses on the FM-1. Third-party modifications and ports are independent projects and are not tested or supported here. Contributions are considered at the maintainer's discretion; no development schedule or ongoing support is promised.

Felucca is developed with the assistance of AI coding agents (code, tests, documentation, translation, maintenance). Design, features, sounds and direction are the maintainer's or the community's; no generative AI is used for music, icons or artwork. See [On AI-Assisted Development and Responsibility](https://github.com/hugelton/Felucca/discussions/166).

## Credits and licence

Felucca is free software under [GPL-3.0-only](LICENSE); the Hügelton Sample Pack is CC0 1.0. The engines and hardware layer retain their own licences. See [CREDITS.md](CREDITS.md) and [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their respective owners. Felucca is not affiliated with, endorsed by or supported by M-VAVE.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
