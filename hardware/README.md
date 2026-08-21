# Hardware

KiCad 10 project for Vallox RS-485 Controller.

| | |
|---|---|
| Board size | — |
| Layers | 2 |
| Copper | 1 oz |
| Finish | HASL lead free (RoHS) |
| Current revision | rev A |

## Open the project

```bash
git submodule update --init          # shared symbol and footprint libraries
open hardware/vallox-rs485-controller.kicad_pro
```

The KiCad project files are named after the project slug (`vallox-rs485-controller.kicad_pro`).
CI expects that name — if the board is named differently, update
`.github/workflows/hardware.yml` to match.

Shared libraries live in `lib/mironet-hw-lib/` and are referenced from
`fp-lib-table` and `sym-lib-table` as `${KIPRJMOD}/lib/mironet-hw-lib/...`.
If symbols appear as missing, the submodule has not been initialised.

## Verification status

| Check | Status | Evidence |
|---|---|---|
| ERC | — | `output/erc_report.json` |
| DRC (CLI) | — | `output/drc_report.json` |
| DRC (GUI, custom rules) | — | run manually before ordering |
| Board built and measured | — | `docs/measurements/` |

## Order packages

Each fabricated revision is frozen under `orders/<rev>-<date>/`: exactly the files
that were sent to the fab, plus what it cost and how long it took. Those folders
are never edited afterwards — corrections go into a new revision.

## Documents

- [`docs/decisions.md`](docs/decisions.md) — component and topology decisions with reasoning
- [`docs/layout-notes.md`](docs/layout-notes.md) — chronological layout log with measured values
