# SPICE — rev A analog islands

ngspice 47 (`brew install ngspice`). `./run.sh` runs every netlist and keeps the
measured lines in `out/` (gitignored). One question per netlist; the question is
the comment block at the top of the file. Findings are written up in
[`docs/measurements/2026-08-23-spice-rev-a.md`](../../docs/measurements/2026-08-23-spice-rev-a.md),
not here.

| Netlist | Question |
|---|---|
| `01-input-inrush.cir` | Inrush into C1 through D1 when the live 22.8 V rail is connected; i²t against the machine's T800 mA fuse |
| `02-en-uvlo.cir` | Buck enable/disable rail voltage through R1/R2; EN pin vs its 7 V absolute maximum |
| `03-buck-ripple.cir` | 5 V ripple, inductor ripple current, RMS current in C1/C2 — open-loop testbench, see below |
| `04-backlight.cir` | Backlight current over the LED Vf window and the rail corners; R11 dissipation; PWM edge |
| `05a-tvs-model-check.cir` | Does the built SM712 model hit the datasheet points (validation only) |
| `05b-bus-miswire.cir` | 22.8 V landed on A/B: current through F1, power in D2, voltage at U4 |
| `05c-bus-surge.cir` | IEC 61000-4-5 surge on A: voltage at U4 vs ±16 V abs max, current vs SM712 IPP |
| `06-ui-dc.cir` | Indicator LED currents over Vf windows; button-ladder ADC levels |

## Models (`models.lib`) — where each one comes from

| Part | Model | Source | Status |
|---|---|---|---|
| D1/D3 SS34 (MDD) | `SS34` | ON Semi MBRS340 parameters (LTspice standard library) | **SUBSTITUTE** — same class (3 A / 40 V Schottky); Vf 0.30 V @ 0.45 A, 0.46 V @ 3 A vs SS34 max 0.55 V @ 3 A |
| D2 SM712 / PSM712 | `SM712_LINE` subckt | Built from the datasheet table: VBR +13.3/−8.3 V @ 1 mA, VC +19/−12 V @ 17 A | **BUILT**, validated in 05a: 13.14 / 19.25 / −8.14 / −12.04 V |
| Q1 AO3400A | `AO3400A` VDMOS | Rds(on) 48 mΩ max @ 2.5 V, 33 mΩ @ 4.5 V, Vth 1.0–1.45 V, Ciss 630 pF from the AOS datasheet | **BUILT** — only Rds(on) and gate charge matter here |
| Backlight string | `BL_LED` | HSD HS20HS072RX spec 3.2: 4 white LEDs parallel, Vf 2.8–3.2 V @ 80 mA; nominal 3.0 V, window as ±0.2 V series offset | **BUILT** |
| D4/D5/D6 | `LED_YG`, `LED_Y`, `LED_R` | Catalogue Vf windows at 20 mA (XL-1608SYGC-06 2.2 V; NCD0603Y1 1.6–2.6 V; KT-0603R 1.8–2.4 V) | **BUILT**, corners as series offsets |
| U2 TPS54202 | — | TI SLVMBJ5 "unencrypted PSpice transient model" is `$CDNENCSTART` Cadence-encrypted and does not load in ngspice | **NOT USED.** 03 is an open-loop synchronous buck (ideal switches Ron 0.19/0.10 Ω + body diodes, fixed duty) — the ripple is L/C physics and does not depend on the loop; load-step response is not simulated |
| F1/F2 JK-mSMD010-60 | resistor | Rmin 0.75 Ω, R1max 15 Ω, Ihold 100 mA, Itrip 300 mA (catalogue) | cold resistance swept; trip time not modelled |
| Panel rail | 22.8 V + 1.8 Ω | `docs/measurements/2026-08-22-rail-voltage-no-load.md` (22.8 V unloaded, 22.0 V @ 0.45 A) | measured |

Vendor files (`vendor/`) are gitignored per the workspace rule; the TI zip is
`https://www.ti.com/lit/zip/SLVMBJ5` should anyone want to try it in PSpice.

## Traps met here (also in the workspace `spice.md`)

- `meas dc … AT=` mis-scales a **current-source** DC sweep; read by index instead (05a).
- `;` inside a `.control` `echo` line ends the command — use commas.
- Resistor currents are not saved by default: put a 0 V source in series and use `vname#branch`.
- An open-loop buck with ideal switches needs **body diodes** across both switches or
  the dead time produces kV spikes (L·di/dt into Roff) that corrupt every average.
- The open-loop output LC (5.8 kHz, Q ≈ 25) rings for milliseconds after start;
  measure switching ripple over two cycles at the end of a long run, not over 1 ms.
