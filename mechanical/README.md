# Enclosure

**Fusion 360 is the master for this enclosure.** The STEP files here are the
published source; `.f3d` files are not committed because they require Fusion,
while STEP opens in every CAD system.

| File | Purpose |
|---|---|
| `step/` | Source geometry, exported from Fusion |
| `stl/` | Ready to print |
| `drawings/` | Dimensioned PDF drawings |
| `print-profiles/` | Slicer profiles (`.3mf`) used for the published prints |
| `draft/` | Parametric first-pass scripts, superseded once Fusion takes over |
| `change-requests/` | Requested dimensional changes with measured justification |

## Printing

| | |
|---|---|
| Material | PETG |
| Layer height | 0.2 mm |
| Walls / infill | — |
| Supports | — |
| Estimated time | — |
| Printer used | Bambu |

## Assembly hardware

| Item | Size | Quantity |
|---|---|---|
| Heat-set insert | M3 | — |
| Screw | M3 × — | — |

Heat-set inserts rather than screwing directly into plastic: direct threads survive
two or three openings, inserts survive the life of the device.

## Fit checks

Every enclosure revision is checked against the actual board 3D model before it is
printed — collisions, clearances, standoffs, antenna keep-out, thermal path and
cable routing. Results with measured values are in `fit-check-<date>.md`.
