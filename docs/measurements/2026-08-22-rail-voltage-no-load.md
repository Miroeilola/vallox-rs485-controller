# Supply rail voltage with the factory panel disconnected (M1, no-load row)

| | |
|---|---|
| Date | 2026-08-22 |
| Hardware revision | none — no board exists |
| Firmware version | none |
| Ambient conditions | indoor, machine running; not recorded further |
| Measured by | Miro Eilola |
| Report written from | a reported reading, not from witnessing the measurement |

## Question

What does the 21 V supply pair (terminals 1 + / 2 −) read with its only load, the
factory panel, disconnected? This is the "factory panel disconnected" row of M1 in
[`../research/measurement-plan.md`](../research/measurement-plan.md). It sets the
upper end of the regulator's input range, and it was made urgent by M2a: the
earlier 22 V reading was taken with the panel's ~10 W on the rail.

## Setup

Multimeter across terminals 1 and 2 at the panel end of the cable, panel
disconnected, machine running. **Not recorded:** multimeter model and range,
machine fan speed, mains voltage at the time, whether the machine showed any
reaction to the missing panel.

## Results

| Measurement | Value | Notes |
|---|---|---|
| Terminal 1–2, factory panel disconnected | **22.8 V DC** | reported reading |
| Terminal 1–2, factory panel connected (~450 mA) | 22 V DC | from the 2026-08-21 report |
| Rise on removing the panel load | ≈ 0.8 V | derived |

## Interpretation

1. **The provisional buck stays.** TPS54202's operating maximum is 28 V. Even
   scaled for mains at +10 % (253 V), an unregulated rail at 22.8 V becomes about
   25 V — 3 V of margin, and the EN divider (100k/15k) then sits at 3.3 V against
   a 7 V absolute maximum. The MP2459 alternative is not needed.
2. **The rail is stiffer than the documents suggested.** A 0.8 V rise for a 10 W
   load step is a few percent regulation. A 14 VA transformer feeding a
   capacitor-input rectifier would move more than that. Together with M2a this
   points the same way: this machine's control supply is larger than the 14 VA
   spare-part listing, or it is not the plain rectified winding the diagrams
   show. It does not need resolving for the design; it is recorded so that nobody
   builds a budget on 14 VA.
3. **The 28.6 V estimate in protocol.md (unknown 3) is withdrawn.** It assumed a
   15 % no-load regulation that this rail does not have.

## Deviations

- Mains voltage not recorded, so the +10 % scaling above is arithmetic on a
  nominal 230 V, not a measurement at 253 V. The margin is large enough that
  this does not change the decision.
- Multimeter model still not recorded — same instrument as the M2a reading;
  one line in [README.md](README.md) when it is at hand.
- What the machine did while the panel was disconnected (kept running, showed a
  fault, stopped) was not recorded. That is item 7 of M3 and it decides the
  installation procedure; if it was observed in passing, it belongs here.
