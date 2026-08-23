# Licensing

This project is released under three licenses, each covering a different kind of work.

| Path | License | SPDX identifier |
|---|---|---|
| `firmware/`, `esphome/`, `scripts/` | MIT License | `MIT` |
| `hardware/`, `mechanical/` | CERN Open Hardware Licence Version 2 – Strongly Reciprocal | `CERN-OHL-S-2.0` |
| `docs/`, `README.md`, images | Creative Commons Attribution-ShareAlike 4.0 International | `CC-BY-SA-4.0` |

Full texts are in [`LICENSES/`](LICENSES/).

Copyright (c) 2026 Miro Eilola / Mironet.

## In practice

- **Software** — use it in anything, including closed commercial products. Attribution
  in your source is appreciated but not required beyond the MIT terms.
- **Hardware and enclosure** — you may build, modify and sell boards based on this
  design, but you must publish your modified design files under CERN-OHL-S-2.0.
- **Documentation** — share and adapt with attribution; derivatives stay under the
  same license.

If these terms do not fit your use case, ask: miro@mironet.fi.

## Third-party material

Component symbols, footprints and 3D models from `lib/mironet-hw-lib/` carry their own
licenses where they originate from third parties; see that repository. Manufacturer
SPICE models and datasheets are **not** redistributed here — the sources are cited in
the documentation instead.
- **Inter** (firmware/components/panel_ui/fonts/): © The Inter Project Authors, SIL Open Font License 1.1 — see fonts/OFL.txt.
- **three.js** (simulator/, via npm): © three.js authors, MIT — https://github.com/mrdoob/three.js/blob/dev/LICENSE. Not vendored; fetched by `npm ci`.
