# Hardware

KiCad 10 project for Vallox RS-485 Controller.

**Status: rev A schematic drawn and ERC clean; rev A placement done, not routed.** The schematic was drawn
on 2026-08-22 ahead of the power-rail measurements (M1, M2a) so that there is
something concrete to review; the power stage is therefore **provisional** and is
marked as such in the title block. What M1/M2a can change: the regulator part
(TPS54202 vs MP2459), the bulk capacitor value, and whether the backlight runs from
a 5 V intermediate rail (as drawn) or a dedicated boost. Everything downstream of
the 3.3 V rail is independent of those measurements. The measurement plan is
[`../docs/research/measurement-plan.md`](../docs/research/measurement-plan.md);
the parts and the reasoning are in
[`../docs/research/component-candidates.md`](../docs/research/component-candidates.md)
and every choice that needed a reason is in [`docs/decisions.md`](docs/decisions.md).

The schematic is a single A3 sheet drawn with continuous wires — every connection
can be followed by eye, and local labels only name nets, they never carry a
connection. Power rails (+5V, +3V3, GND) use power symbols.

| | |
|---|---|
| Board size | 104 × 66 mm, 2 mm corner radius, 4 × M3 (Ø3.2 mm NPTH) |
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
| ERC | 0 errors, 1 warning (`lib_symbol_mismatch` on Q1, cosmetic) — 2026-08-22, KiCad 10.0.2 | `output/erc_report.json` |
| DRC (CLI) | 0 errors, 6 warnings (`silk_edge_clearance` on edge-mounted U1/J1/J2), 138 unconnected — placement only, not routed — 2026-08-22, KiCad 10.0.2 | `output/drc_report.json` |
| DRC (GUI, custom rules) | — | run manually before ordering |
| Board built and measured | — | `docs/measurements/` |

## Order packages

Each fabricated revision is frozen under `orders/<rev>-<date>/`: exactly the files
that were sent to the fab, plus what it cost and how long it took. Those folders
are never edited afterwards — corrections go into a new revision.

## Documents

- [`docs/decisions.md`](docs/decisions.md) — component and topology decisions with reasoning
- [`docs/layout-notes.md`](docs/layout-notes.md) — chronological layout log with measured values
