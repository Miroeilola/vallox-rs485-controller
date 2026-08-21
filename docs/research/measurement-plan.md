# Measurement plan

What has to be measured on the real machine before the schematic is drawn, in the
order it has to happen, and what result confirms or kills each claim in
[protocol.md](protocol.md).

Each step writes a report into `docs/measurements/` in the format required by the
workspace rules. A step is not done because it was attempted — it is done when its
report exists with numbers in it.

## Safety assessment

The machine is a mains appliance. Its connection box carries 230 V and, on units
with electric pre- and post-heating, up to 2.4 kW of heater load on a 16 A supply.

Rules for every step below:

1. **The machine is disconnected at the plug before any cover comes off**, and it
   stays disconnected while anything is being wired. The plug lead is 1.8 m from the
   top of the machine and is the disconnection means.
2. **Nothing is connected to the panel terminals until step M1 is complete**,
   including instruments. M1 exists to find out what those terminals actually are.
3. **The (+) terminal is treated as destructive.** The manufacturer states that
   miswiring it destroys the panel. Every connection to terminal 1 is checked twice
   against the terminal numbering before power is applied, and the first power-up of
   any new circuit happens on the bench from a current-limited supply, not on the
   machine.
4. **The house is occupied and the ventilation is in use.** Test windows are agreed
   in advance and kept short. The machine is left in a working state at the end of
   every session, verified from the factory panel, not assumed.
5. **No writes to the bus until M5.** The bus controls a real appliance. A wrong
   frame does not produce an error message, it changes what the machine does.

## M0 — Identify the machine

*No instruments. Cannot be skipped.*

Photograph the model plate, the connection box with the terminal strip visible, the
existing panel front and its terminal block with the wires still connected, and the
fan assembly. Record model, year, serial, panel model, panel address, which optional
sensors are fitted, and whether the fans are AC or DC.

**Result:** the table in [target.md](target.md) is filled in. Everything downstream
depends on it — the register set, whether 0xB0/0xB1 exist, which address is free.

## M1 — Is the panel bus safe to touch, and what is on it

*Multimeter with a CAT III rating. Machine powered.*

| Measurement | Expected | What it decides |
|---|---|---|
| Terminal 2 (−) to protective earth, AC and DC | Near 0 V AC, small DC offset | Claim 3. **If a significant AC voltage appears, the panel bus is mains-referenced and this project stops in its present form.** |
| Terminal 5 (M) to protective earth, AC and DC | Same | As above |
| Terminal 1 to terminal 2, machine idle | approx. 21 V DC, per the manual | Claim 2 vs claim 5 (24 V) |
| Same, machine at fan speed 8, heating on | Sags by some amount | Lower end of the input range |
| Same, factory panel disconnected | Rises by some amount | Upper end of the input range, no-load |
| Ripple on terminal 1–2, oscilloscope, AC coupled | Unknown | Whether the supply is regulated or a rectified transformer winding |
| A and B to M, DC, bus idle | Inside −7…+12 V | Claim 4, and whether the bus is biased |
| A–B differential, bus idle | Some hundreds of mV if biased, near 0 if not | Whether this board needs fail-safe biasing |

**This step is the gate for everything else.** No further work happens until the
first two rows are known.

## M2 — How much current can the rail give

*Bench supply not usable here — the source is the machine. Electronic load or a
resistor decade, plus a multimeter, plus the oscilloscope on the rail.*

Load the 21 V rail in steps with the factory panel connected and the machine
running, watching the rail voltage and watching the factory panel for a bus fault or
a reset. Find the current at which either the rail collapses or the machine
complains, and stay well below it.

**Result:** the power budget. If the usable headroom turns out to be smaller than an
ESP32-S3's Wi-Fi transmit peak plus margin, the design changes — either a large bulk
reservoir sized from the measured burst profile, or a lower-power radio choice, or
the board stops being bus-powered. This is the single number that most affects the
bill of materials, and it is not in any datasheet.

## M3 — Passive capture

*USB RS-485 adapter with the driver disabled or a receive-only adapter, connected
A/B/M only. Nothing connected to terminals 1 and 2. Logic analyser or oscilloscope
in parallel.*

The adapter must not be able to transmit. This is checked on the bench first by
connecting it to a second adapter and confirming that nothing comes out of it while
the software runs.

Capture, with the factory panel connected and operating normally:

1. Idle traffic, 30 minutes.
2. The panel's start-up burst, from machine power-on.
3. A fan speed change, each step 1→8 and back.
4. Each panel button pressed once.
5. If the unit has them: a CO₂ or RH sensor exchange, looking for 0x91 / 0x8F.

**Verifies:** claims 1, 6, 7, 8, 9, 10, 11, 15, and which temperature register set
this machine uses. From the oscilloscope trace: the bit period, the differential
swing, the common-mode level, the rise time, and whether the idle state is properly
biased.

**Result:** `docs/measurements/<date>-bus-capture.md` with the scope images, and a
raw capture file kept in the repository — a decoded reference capture is something
none of the existing projects publish.

## M4 — Timing

*From the M3 captures. No new hardware.*

Measure, as distributions rather than single numbers: the gap between the last byte
of one frame and the first byte of the next; the delay from a poll to its answer;
the panel's poll period; and the longest silent gap in a 30-minute capture.

**Result:** the real value behind claim 12. The 100 ms figure in existing software is
a chosen constant, not a measurement, and this is where it gets replaced by
something with evidence behind it. It also sets the RS-485 driver-enable timing —
how long after the last stop bit the driver may stay on.

## M5 — First transmission

*Only after M1–M4 are reported. Poll frames only. A write allow-list in the software
that is empty.*

Send one poll for register 0xA3 to 0x11, from an address confirmed unused in M3.
Confirm the answer, confirm the factory panel does not report a bus fault, confirm
the machine keeps running.

Then poll each read-only register from the map and record which ones answer. A
register that does not answer on this machine is not in this machine's map, and that
is a finding worth publishing per model.

**Result:** the verified register map for this specific machine, and the answer to
whether a second client is tolerated at all.

## M6 — First write

*Only after M5. One register. Fan speed. Nothing else.*

Set fan speed to the value it already has, and confirm nothing changes. Then step it
by one and confirm the factory panel shows the same value. Then set it back.

The write allow-list in the firmware stays at exactly one register until this report
exists, and every register added to it later needs its own entry in the report.

**Result:** claim 19 is contained by construction rather than trusted.

## What is deliberately not tested

- Duplicate panel addresses, claim 13. The failure mode is a machine in a fault
  state and there is nothing to learn from it.
- Writing any register whose meaning is only supported by a single source.
- Anything on the mains side of the machine.
- Long-term behaviour of anything, at this stage. That belongs to bringup.

## Instruments

To be recorded in `docs/measurements/README.md` with model numbers before the first
report is written. A measurement whose instrument is unnamed cannot be judged by a
reader, so this list is part of the evidence, not paperwork.
