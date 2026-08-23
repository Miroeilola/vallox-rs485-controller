# Current drawn by the factory panel from the 21 V rail (M2a)

| | |
|---|---|
| Date | 2026-08-22 |
| Hardware revision | none — no board exists |
| Firmware version | none |
| Ambient conditions | indoor, machine running; not recorded further |
| Measured by | Miro Eilola |
| Report written from | a reported reading, not from witnessing the measurement |

## Question

How much current does the original Vallox DIGIT SE LED panel draw from the supply
pair (terminals 1 + / 2 −)? This is measurement **M2a** in
[`../research/measurement-plan.md`](../research/measurement-plan.md): the panel's
draw is current the rail has demonstrably been delivering, so it is the floor of
the rail's capability without loading the rail with anything new.

## Setup

Ammeter in series with the + conductor at the panel terminal, factory panel
connected and operating, machine running.

**Not recorded — to be filled in by the person who measured:** instrument make,
model and range; whether the meter was in the + or − conductor; machine fan speed
and heating state; what the panel's LEDs were showing; how long the momentary
peak lasted and what caused it (power-on, a button press, an LED change); whether
both figures are DC readings.

## Results

| Measurement | Value | Notes |
|---|---|---|
| Panel supply current, continuous | **450 mA** | reported reading |
| Panel supply current, momentary peak | **700 mA** | reported reading; duration and trigger not recorded |
| Supply voltage | 22 V DC | from the 2026-08-21 report, load state not recorded there either |

Derived, not measured: 450 mA × 22 V ≈ **9.9 W** continuous, 700 mA × 22 V ≈
15 W momentary, drawn from the bus by the panel alone.

## Interpretation

1. **The rail supplies the replacement with room to spare.** The board's own
   draw at the rail is on the order of 0.1–0.2 A (ESP32-C3 Wi-Fi peaks, 80 mA
   backlight at 5 V, RS-485, through a buck at ~85 %). The rail has been feeding
   two to four times that, continuously, for two decades. Risk R2 in
   [`../research/risks.md`](../research/risks.md) is closed by this reading, subject
   to the conditions above being filled in.
2. **The 60 mA prior was wrong by 7×.** [`../research/user-interface.md`](../research/user-interface.md)
   inferred ~60 mA / 1.3 W from the size of the regulator's heatsink. That
   inference is withdrawn there and here. It was an estimate labelled as one, and
   this is what M2a was for.
3. **"Draws less while doing more" is now a measurable 2–4× claim**, not a hope.
   The bring-up report compares the finished board's rail current against 450 mA.
4. **The no-load rail voltage matters more than it did.** 22 V was read with
   (presumably) a 10 W load on an unregulated, rectified 16 VAC secondary. Remove
   the panel and the rail rises — by how much depends on the transformer's
   regulation, and the provisional buck's operating maximum is 28 V. The
   "factory panel disconnected" row of M1 is now the measurement that decides
   the buck, and it is a one-minute job.

## Deviations

- **The reading disagrees with the documents, and the disagreement is recorded
  rather than explained away.** The Vallox diagrams (see
  [`../research/sources.md`](../research/sources.md)) put the control supply on a
  **14 VA** transformer with a **T800 mA** fuse on its 16 VAC secondary, shared
  with the mainboard and rated for up to three panels. One panel at ~10 W DC is
  roughly 0.7–0.9 A rms on that secondary before the mainboard's own share. Either
  this machine's transformer is larger than the spare-part listing (a 1.3 A / 16 V
  variant exists from 04/2002), or most of the panel current bypasses the small
  regulator (LEDs fed from the rail, which the heatsink size supports), or the
  reading carries an artifact (AC range, shunt burden, inrush read as
  continuous). The first two are consistent with each other; the third is ruled
  out only by recording the instrument and repeating the reading. **None of the
  three changes the conclusion in 1.**
- A linear regulator passing 450 mA across 17 V would dissipate 7.6 W; the
  folded heatsink in the 2026-08-21 photographs cannot hold that. Most of the
  panel's current therefore does not go through the regulator. Worth a look at
  the panel board when it is next open: where the LED strings are fed from.
- Measurement conditions not recorded — see *Setup*. The figures are recorded
  as reported and are enough for the conclusions drawn; they are not enough for
  the datasheet until the instrument and the load state are attached.
