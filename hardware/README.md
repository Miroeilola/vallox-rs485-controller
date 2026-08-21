# Hardware

KiCad 10 project for Vallox RS-485 Controller.

**There is no KiCad project here yet, and that is deliberate.** Two measurements
decide the power stage and neither is answerable from a document: whether the
panel's 21 V rail is isolated from mains, and how much current it can supply.
Drawing a schematic before those exist would mean drawing it twice. The plan is
[`../docs/research/measurement-plan.md`](../docs/research/measurement-plan.md);
the parts that have been costed and the reasoning behind them are in
[`../docs/research/component-candidates.md`](../docs/research/component-candidates.md).

| | |
|---|---|
| Board size | — |
| Layers | 2 (assumed; not fixed until the placement is known) |
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
