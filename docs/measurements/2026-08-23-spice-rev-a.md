# SPICE: rev A analog islands before ordering

Date 2026-08-23 · Hardware rev A (main 360c0ea, routed, sourced) · Firmware n/a ·
**Simulated, not measured.** ngspice 47, netlists in `hardware/sim/`, models and
their provenance in `hardware/sim/README.md`.

## Question

Before the board is ordered: do the analog islands behave as the schematic
assumes — input front end, buck enable and ripple, backlight drive, RS-485
protection, indicator LEDs and the button ladder — and where are the margins thin?
One sub-question per netlist; each is stated at the top of its `.cir` file.

## Setup

ngspice 47 batch runs (`hardware/sim/run.sh`). Component values from the rev A
schematic netlist (exported 2026-08-23), part parameters from the catalogue
entries in `hardware/docs/parts-revA.csv`, the panel rail from the 2026-08-22
measurements (22.8 V unloaded, 1.8 Ω source from the 0.8 V / 0.45 A step). The
TPS54202 is **not** modelled (TI's package is Cadence-encrypted); the buck bench is
open-loop and answers only the ripple/RMS questions. The SM712 model was built from
its datasheet table and validated against it (13.14 / 19.25 / −8.14 / −12.04 V vs
13.3 / 19 / −8.3 / −12). The SS34 is a substitute model of the same device class.

## Results

### 01 — Inrush when the live rail is connected (C1 220 µF through D1)

| Source R | I peak | t to 20.5 V | i²t | Charge |
|---|---|---|---|---|
| 0.5 Ω | 32 A | 0.47 ms | 0.067 A²s | 5.0 mC |
| **1.8 Ω (measured)** | **11.3 A** | **1.2 ms** | **0.026 A²s** | 5.0 mC |
| 5 Ω | 4.3 A | 2.9 ms | 0.011 A²s | 5.0 mC |

### 02 — Buck enable through R1/R2 (100 k / 15 k)

Start 9.28 V typ (8.9–9.7 V over the EN threshold window), stop ≈ 8.5 V.
V(EN) = 3.26 V at 25 V rail, 3.65 V at 28 V; the 7 V absolute maximum is reached
at a 54 V rail.

### 03 — Buck ripple and input-capacitor RMS current (open-loop bench, 22.8 V in)

| Load | Vout ripple @ 500 kHz | L1 ripple | C1 RMS | C2 RMS | Vin ripple |
|---|---|---|---|---|---|
| 0.15 A | 2.8 mVpp | 0.37 App | 36 mA | 71 mA | 41 mVpp |
| 0.25 A | 2.8 mVpp | 0.37 App | 55 mA | 98 mA | 60 mVpp |
| 0.45 A | 2.8 mVpp | 0.36 App | 96 mA | 162 mA | 100 mVpp |

Hand calculation for the inductor ripple: (22.8 − 5.08) × 0.223 / (22 µH × 500 kHz)
= 0.36 A. Inductor peak at 0.45 A load 0.63 A vs 2.2 A saturation. C1 (Nichicon
UCD, 300 mA rated) at ≤ 96 mA.

### 04 — Backlight current, R11 = 27 Ω from the 5 V rail, Q1 on

| Rail | Vf 2.8 V | Vf 3.0 V | Vf 3.2 V | P(R11) max |
|---|---|---|---|---|
| 5.08 V (buck) | 84 mA | 77 mA | 71 mA | 0.19 W |
| 4.98 V (buck −2 %) | 80 mA | 74 mA | 68 mA | 0.17 W |
| 4.6 V (USB only, through D3) | 68 mA | 62 mA | 55 mA | 0.13 W |

Q1 Vds 4–5 mV when on; the PWM edge through R12 = 100 Ω reaches 90 % of the on
current ~40 ns after the 3.3 V gate step.

### 05b — 22.8 V landed on the A (or B) terminal

| F1 cold R | I | V at U4 pin | P in D2 | P in F1 |
|---|---|---|---|---|
| 0.75 Ω | 3.2 A | 14.6 V | 47 W | 7.7 W |
| 3 Ω | 1.8 A | 14.1 V | 26 W | 9.8 W |
| 15 Ω | 0.55 A | 13.6 V | 7.4 W | 4.5 W |

(with the 1.8 Ω rail source; the machine's T800 mA fuse is upstream of that and
not modelled)

### 05c — IEC 61000-4-5 surge on A (42 Ω source, F1 cold 0.75 / 3 Ω)

| Surge | I peak into D2 | V at U4 pin | E in D2 |
|---|---|---|---|
| +1 kV | 22–23 A | **+20.9 … +21.3 V** | 23–24 mJ |
| +500 V | 11 A | +17.2 … +17.4 V | 10 mJ |
| −1 kV | 22–23 A | −13.1 … −13.3 V | 15 mJ |

### 06 — Indicator LEDs (1 kΩ from 3.3 V) and the button ladder

| LED | Vf min corner | nominal | Vf max corner |
|---|---|---|---|
| PWR yellow-green XL-1608SYGC-06 | 1.43 mA | 1.24 mA | 1.05 mA |
| BUS yellow NCD0603Y1 | 1.91 mA | 1.43 mA | 0.95 mA |
| FAULT red KT-0603R | 1.71 mA | 1.43 mA | 1.14 mA |

Button ladder (R22 10 k pull-up, 100 n): none 3.30 V · SW1 0.00 V · SW2 0.430 V ·
SW3 0.819 V · SW4 1.336 V. Smallest step 0.39 V (SW2→SW3).

## Interpretation

- **Inrush is a non-event for the machine.** 0.026 A²s against a T800 mA fuse whose
  melting i²t is of the order of a few A²s; 11 A for ~0.1 ms against the SS34's
  surge rating (80 A, 8.3 ms half-sine). The rail itself is in regulation again
  after ~1.2 ms.
- **EN divider is right:** starts at ~9.3 V, far below the 20 V+ the rail actually
  sits at, and 3.3 V on the pin at the 25 V worst case against a 7 V limit.
- **Buck passives are sized with margin:** 2.8 mV switching ripple, inductor at
  30 % of saturation at the heaviest load, bulk-cap ripple current at a third of
  its rating. The load-step response (Wi-Fi bursts) is not simulated — the loop is
  TI's and the model is closed; it is measured at bring-up instead.
- **Backlight lands on 71–84 mA at the nominal rail** against the module's 80 mA
  spec point; on USB alone it runs 55–68 mA (dimmer, acceptable for bench use).
  **R11 dissipates up to 0.19 W, 75 % of a 1206 thick-film's 0.25 W rating at 100 %
  duty.** Acceptable at room temperature, thin in a warm enclosure at full duty.
- **A supply miswire onto A/B is not survivable for D2.** Even with the PTC at its
  15 Ω maximum, the SM712 (a SOT-23, 600 W for 8/20 µs) sits at 7–47 W for the
  tens to hundreds of milliseconds the PTC and the machine's fuse take to act. The
  common TVS failure mode is short; then F1 trips and U4 sees ~0 V — the line is
  dead but the board lives. If D2 fails open, U4's pin gets the rail through the
  tripped PTC. This is the usual fate of a transceiver-side TVS under a DC
  miswire and it is recorded rather than designed away: the fix is either a much
  larger TVS or a series element that can take the power, neither of which fits a
  rev A that keeps the installer's five terminals.
- **Surge:** at +1 kV the clamp current (23 A) exceeds the SM712's 17 A IPP and
  U4's pin reaches 21 V for microseconds against a ±16 V absolute maximum; at
  +500 V the TVS is within rating and the pin sees 17.4 V; the negative side is
  within limits at −1 kV. The bus is an in-house signal line a few metres long
  behind the machine; 500 V (installation class 2) is the realistic level and
  even that is 1.4 V over the DC absolute maximum for ~10 µs. **Rev B candidate:**
  10 Ω 0603 in series between the TVS node and each U4 bus pin, the classic second
  stage; it does not fit rev A's routing.
- **Indicator LEDs are lit but dim.** 1–1.9 mA over the Vf windows; the yellow
  reaches 0.95 mA at its Vf maximum. Brightness scales with the catalogue
  luminous intensity at 20–25 mA: the yellow-green PWR part (60 mcd at 25 mA) will
  give roughly 3 mcd at 1.2 mA — visible, not conspicuous; BUS ~12 mcd, FAULT
  ~20 mcd. **R20 can drop to 330 Ω (3.6 mA, ~9 mcd) on the same footprint** if
  PWR is too faint at bring-up; the GPIO sources that comfortably.
- **Button ladder** codes are 0.39 V apart at minimum — over 100 LSB on the 12-bit
  ADC with 1 % resistors; the un-pressed 3.3 V saturates the 11 dB range, which is
  fine for "none".

## Deviations

- TPS54202 not simulated with its own model (encrypted package). Load-step and
  start-up behaviour remain bring-up measurements.
- D2 miswire case and +1 kV surge are the two findings that exceed a rating; both
  are recorded as accepted for rev A with the rev B candidates above. The PTC trip
  time and the machine's fuse are not modelled — they decide how long the
  overload lasts, not whether it exceeds the TVS.
- The SS34 model is a substitute (MBRS340 parameters); it affects only the inrush
  peak (diode drop ≈ 0.5 V at 11 A) by a few percent.
- R11 power margin (75 %) is noted, not changed: a 0.5 W 1206 in the same footprint
  is the cheap alternative if the enclosure runs warm.

## Revision history

| Rev | Date | Change |
|---|---|---|
| 1 | 2026-08-23 | First run of all eight benches |
