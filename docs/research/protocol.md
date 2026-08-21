# Vallox DIGIT RS-485 protocol — what is known and how well

This file separates what has been read from what has been proven. Nothing here has
been measured yet, so **every row is an open question** until the captures in
[measurement-plan.md](measurement-plan.md) exist. Confidence ratings are defined in
[README.md](README.md); sources are listed in [sources.md](sources.md).

The register semantics originate from a Vallox document titled *DIGIT
väyläprotokolla*, translated and circulated as `Digit_protocol_english_RS485.pdf`.
That document could not be retrieved directly. What is written below is the
intersection of four independent implementations that each claim to follow it, and
it is rated accordingly.

## Claims

| # | Claim | Source | Confidence | How it will be verified |
|---|---|---|---|---|
| 1 | RS-485, half duplex, 9600 baud, 8 data bits, no parity, 1 stop bit | Vallox LON gateway note; all four implementations | `manufacturer` | Oscilloscope: measure one bit period, expect 104.2 µs ±2 % |
| 2 | Panel supply is approx. 21 VDC on terminals 1(+) / 2(–) | Vallox Digit2 SE manual | `manufacturer` | Multimeter, machine idle and at fan speed 8, panel connected and disconnected |
| 3 | The 21 V comes from a protective-voltage winding and is isolated from mains | Vallox Digit2 SE internal wiring diagram, low-resolution scan | `anecdotal` (reading of a poor scan) | Measure terminal 2 and terminal 5 to protective earth, AC and DC, before anything else is connected |
| 4 | Terminal 5 (M, cable screen) is the signal reference for A/B | Vallox Digit2 SE manual | `manufacturer` | Measure DC offset of A and B against M; expect an idle common-mode voltage inside the RS-485 −7…+12 V window |
| 5 | Community reports 24 V, not 21 V, at the machine terminal | Creating Smart Home | `anecdotal` | Same measurement as claim 2. Design input range must cover both readings and their tolerance |
| 6 | Every telegram is exactly 6 bytes | All four implementations | `reverse-engineered` | Capture: byte-gap timing shows a clear inter-frame gap after every 6th byte |
| 7 | Byte 0 (domain / system) is always 0x01 | All four implementations | `reverse-engineered` | Capture: histogram of byte 0 over a long capture |
| 8 | Checksum is the low byte of the sum of the first five bytes | All four implementations | `reverse-engineered` | Capture: verify over thousands of frames, count failures |
| 9 | Mainboard is 0x11; 0x10 addresses all mainboards | All four implementations | `reverse-engineered` | Capture: source-address histogram |
| 10 | Panels are 0x21…0x29, 0x20 addresses all panels, LON module is 0x28 | windkh; ioBroker (states panels 1–9) | `reverse-engineered` | Capture with the factory panel connected — its address will appear |
| 11 | A poll is a frame with byte 3 = 0x00 and byte 4 = the register wanted | All four implementations | `reverse-engineered` | Capture: panel startup burst, then reply correlation |
| 12 | The bus has no arbitration; a client waits for ~100 ms of silence before transmitting | pvainio (implementation constant), inferred | `reverse-engineered` | Capture: measure the real inter-frame gap distribution before choosing this number |
| 13 | Duplicate panel addresses put the machine into a bus-fault state | Vallox Digit2 SE manual | `manufacturer` | Not to be tested deliberately on the live machine |
| 13b | Two controllers on this bus override each other, so a third-party client cannot coexist with a factory panel | Prior testing on the target machine | `measured, first-hand` | Established. It is why this device replaces the panel — see the decision record |
| 20 | The supply pair carries 22 V on the target machine | Measured 2026-08-21, instrument and load state not recorded | `measured, conditions incomplete` | Repeat at no load and at full machine load, with the instrument recorded |
| 21 | The original panel's transceiver is a MAX487: 1/4 unit load, 48 kΩ, slew-rate limited to 250 kbps | Read off the board, plus the Analog Devices datasheet | `manufacturer` | Established. The replacement matches or betters it |
| 14 | Temperatures are NTC 5 kΩ raw counts mapped through a fixed 256-entry table | windkh, pvainio, Tom-Bom-badil (identical tables) | `reverse-engineered` | Compare a reported value against a calibrated reference thermometer in the same airflow |
| 15 | Fan speed is a thermometer-coded bit mask, 1→0x01 … 8→0xFF | All four implementations | `reverse-engineered` | Capture while changing speed on the factory panel |
| 16 | Relative humidity is `(x − 51) / 2.04` percent, valid from 0x33 | windkh, pvainio | `reverse-engineered` | Only if the target unit has an RH sensor |
| 17 | CO₂ is two bytes, 0x2B high and 0x2C low, polled low-then-high by the panel | kotope, windkh | `reverse-engineered` | Only if the target unit has a CO₂ sensor |
| 18 | Broadcast 0x91 / 0x8F suspend and resume bus traffic during CO₂ sensor exchanges, sent twice | windkh | `reverse-engineered` | Capture: look for these register ids in a long capture on a unit with CO₂ sensors |
| 19 | Writing an unknown register or an out-of-range value can damage the unit | Vallox documentation, as quoted by pvainio | `anecdotal` | Not verifiable — treated as true and handled by a firmware allow-list |

## Frame format

Six bytes, no framing characters, no escaping. Frame boundaries come from the
inter-byte gap and from the checksum, which is why a receiver has to resynchronise
by discarding one byte at a time until a checksum passes.

```
  byte 0   byte 1   byte 2   byte 3   byte 4   byte 5
+--------+--------+--------+--------+--------+--------+
| domain | sender |receiver|register| value  |checksum|
+--------+--------+--------+--------+--------+--------+
   0x01                                        low byte of sum of bytes 0..4
```

Read (poll): `register = 0x00`, `value = the register being asked for`.
Write, and the reply to a poll: `register = the register`, `value = its value`.

Worked example, a panel at 0x21 polling the mainboard at 0x11 for register 0xA3:

```
01 21 11 00 A3 D6      D6 = (01 + 21 + 11 + 00 + A3) & 0xFF = 214
```

**The source this example comes from writes the checksum as `C9`.** It is `D6`.
The arithmetic is not close — 201 against 214 — so it is a transcription error in
an annotation rather than a different checksum rule, and the two implementations
that compute the checksum in code both compute it the way it is written above.

The mistake is recorded rather than quietly corrected because it is the concrete
form of what the confidence ratings in this file mean. Four sources agreeing does
not make a claim measured; it makes it four copies of the same transcription, and
one of them has a wrong number in it. Found by a unit test in
`firmware/components/vallox_protocol`, which is why that code exists before any
hardware does.

## Addresses

| Address | Device |
|---|---|
| 0x10 | all mainboards (broadcast) |
| 0x11 | mainboard 1 — the machine |
| 0x20 | all panels (broadcast) |
| 0x21…0x27, 0x29 | panels 1…8 |
| 0x28 | LON gateway module |

**This device replaces the panel rather than joining it**, so it takes the address
the panel used — 0x21 on the target machine. It does not need to find a free slot,
because there is no other panel to avoid.

That is not a preference. Coexistence was tested on the target machine and two
controllers override each other; the manufacturer's "up to three panels" holds for
three Vallox panels, which the mainboard keeps in step by broadcasting to the whole
panel group, and not for a client that polls on its own schedule.

Existing software picks an address by convention and hopes — pvainio defaults to
0x27, kotope uses 0x22, Tom-Bom-badil uses 0x2E and 0x2F, outside the documented
panel range. None of that applies here.

What the firmware still does is **listen before it speaks**, but for a different
reason: to confirm that nothing else is talking on the panel side. Two devices on
one address put the machine into a bus-fault state, and finding that out by
provoking it is the expensive way. The check is a precondition, not a convenience.

## Register map

Compiled from four implementations. The Read/Write column is what the sources
claim, not what has been tested. `dec` means the raw byte is the value.

### Measurements — read only

| Reg | Name | Encoding |
|---|---|---|
| 0x32 | Outdoor air temperature | NTC table |
| 0x33 | Exhaust air temperature (to outside) | NTC table |
| 0x34 | Extract air temperature (from inside) | NTC table |
| 0x35 | Supply air temperature (to inside) | NTC table |
| 0x2A | Highest measured RH of 0x2F and 0x30 | `(x−51)/2.04` % |
| 0x2F | RH sensor 1 | `(x−51)/2.04` % |
| 0x30 | RH sensor 2 | `(x−51)/2.04` % |
| 0x2B | CO₂ measurement, high byte | pair with 0x2C |
| 0x2C | CO₂ measurement, low byte | pair with 0x2B |
| 0x2D | Which CO₂ sensors are installed | bits 1…5 |
| 0x2E | Incoming current/voltage signal, mA | dec |
| 0x36 | Last fault number | see fault table |
| 0x55 | Post-heating on-time counter | percent = x / 2.5 |
| 0x56 | Post-heating off-time counter | percent = x / 2.5 |
| 0x57 | Post-heating target | NTC table |
| 0x79 | Fireplace/boost minutes remaining | dec |
| 0xAB | Months until next service reminder | dec |

The older units in the field are also reported to broadcast temperatures on
0x58 (supply-side outdoor), 0x5A, 0x5B and 0x5C — pvainio handles both sets and
calls 0x32…0x35 "the newer protocol". Which set a given machine uses is a property
of the machine and has to be discovered from a capture, not assumed.

### Control — read and write

| Reg | Name | Encoding |
|---|---|---|
| 0x29 | Current fan speed | bit mask, 1…8 |
| 0xA5 | Maximum allowed fan speed | bit mask, 1…8 |
| 0xA9 | Fan speed after power-on | bit mask, 1…8 |
| 0xA4 | Heating setpoint | NTC table |
| 0xA7 | Pre-heating setpoint | NTC table |
| 0xA8 | Supply fan stop temperature (frost protection) | NTC table, −6…+15 °C |
| 0xAF | Heat-recovery cell bypass setpoint | NTC table |
| 0xB2 | Defrost hysteresis | `setpoint + x/3`, so 0x03 = 1 °C |
| 0xAE | Basic humidity level | `(x−51)/2.04` % |
| 0xA6 | Service reminder interval, months | dec |
| 0xB0 | DC supply fan adjustment, % | dec, 65…100 |
| 0xB1 | DC exhaust fan adjustment, % | dec, 65…100 |
| 0xB3 | CO₂ setpoint high byte | pair with 0xB4 |
| 0xB4 | CO₂ setpoint low byte | pair with 0xB3 |

### Bit registers

**0xA3 — panel state.** Bits 0…3 are settable and correspond to the four panel
LEDs; bits 4…7 are the indicator icons and are read only.

| Bit | Meaning | Access |
|---|---|---|
| 0 | Power on | R/W |
| 1 | CO₂ control enabled | R/W |
| 2 | RH control enabled | R/W |
| 3 | Heating mode — 1 = winter (heat recovery), 0 = summer (bypass) | R/W |
| 4 | Filter guard warning | R |
| 5 | Pre/post heating active | R |
| 6 | Fault | R |
| 7 | Service reminder | R |

**0x06 — fan speed relay image.** Bits 0…7 mirror the eight speed relays, read only.
This is the autotransformer tap image and is the reason fan speed is thermometer
coded.

**0x07 — multipurpose 1.** Bit 5 = post-heating on. Read only.

**0x08 — multipurpose 2.**

| Bit | Meaning | Access |
|---|---|---|
| 1 | Damper motor position, 0 = winter, 1 = summer | R |
| 2 | Fault relay, 0 = open, 1 = closed | R |
| 3 | Supply fan, 0 = on, 1 = off | R/W (reported to need writing twice) |
| 4 | Pre-heating | R |
| 5 | Exhaust fan, 0 = on, 1 = off | R/W (reported to need writing twice) |
| 6 | Fireplace/boost switch input | R |

**0x6D — flags 2.** Bit 0 CO₂ speed-up request, bit 1 CO₂ speed-down request,
bit 2 RH speed-down request, bit 3 switch speed-down request, bit 6 CO₂ alarm,
bit 7 frost risk at the sensor.

**0x6F — flags 4.** Bit 4 water coil frost risk, bit 7 master/slave selection.

**0x70 — flags 5.** Bit 7 pre-heating status, 0 = on, 1 = off.

**0x71 — flags 6.** Bit 4 remote monitoring active (R), bit 5 fireplace/boost
activation (R/W — read the register, set the bit, write it back), bit 6
fireplace/boost running (R).

**0xAA — program.** Bits 0…3 CO₂/RH adjustment interval in minutes, bit 4
automatic RH base level search, bit 5 switch mode (1 = boost, 0 = fireplace),
bit 6 radiator type (0 = electric, 1 = water), bit 7 cascade control.

**0xB5 — program 2.** Bit 0 max speed limit behaviour.

### Bus control

| Reg | Name |
|---|---|
| 0x91 | SUSPEND — broadcast twice, pauses traffic for a CO₂ sensor exchange |
| 0x8F | RESUME — broadcast twice |
| 0xC0 | Queried at startup, answered with 0x03, undocumented |

### Fault numbers, register 0x36

| Value | Meaning |
|---|---|
| 0x00 | none |
| 0x05 | supply air sensor fault |
| 0x06 | CO₂ alarm |
| 0x07 | outdoor air sensor fault |
| 0x08 | extract air sensor fault |
| 0x09 | water coil frost risk |
| 0x0A | exhaust air sensor fault |

## NTC temperature table

256 entries, raw byte → °C. Identical in windkh, pvainio and Tom-Bom-badil, which
is the strongest agreement in this document. The sensors are reported to be
NTC 5 kΩ. The table is non-linear and saturates at both ends, so a raw value of
0x00 or 0xFF means "out of range", not "−74 °C" or "100 °C" — the firmware must
treat the saturated ends as invalid rather than publishing them as readings.

```
0x00  -74 -70 -66 -62 -59 -56 -54 -52 -50 -48 -47 -46 -44 -43 -42 -41
0x10  -40 -39 -38 -37 -36 -35 -34 -33 -33 -32 -31 -30 -30 -29 -28 -28
0x20  -27 -27 -26 -25 -25 -24 -24 -23 -23 -22 -22 -21 -21 -20 -20 -19
0x30  -19 -19 -18 -18 -17 -17 -16 -16 -16 -15 -15 -14 -14 -14 -13 -13
0x40  -12 -12 -12 -11 -11 -11 -10 -10  -9  -9  -9  -8  -8  -8  -7  -7
0x50   -7  -6  -6  -6  -5  -5  -5  -4  -4  -4  -3  -3  -3  -2  -2  -2
0x60   -1  -1  -1  -1   0   0   0   1   1   1   2   2   2   3   3   3
0x70    4   4   4   5   5   5   5   6   6   6   7   7   7   8   8   8
0x80    9   9   9  10  10  10  11  11  11  12  12  12  13  13  13  14
0x90   14  14  15  15  15  16  16  16  17  17  18  18  18  19  19  19
0xA0   20  20  21  21  21  22  22  22  23  23  24  24  24  25  25  26
0xB0   26  27  27  27  28  28  29  29  30  30  31  31  32  32  33  33
0xC0   34  34  35  35  36  36  37  37  38  38  39  40  40  41  41  42
0xD0   43  43  44  45  45  46  47  48  48  49  50  51  52  53  53  54
0xE0   55  56  57  59  60  61  62  63  65  66  68  69  71  73  75  77
0xF0   79  81  82  86  90  93  97 100 100 100 100 100 100 100 100 100
```

Resolution is about 1 °C around room temperature and coarser at the extremes. The
inverse conversion, needed when writing a setpoint, is not unique — several raw
values map to the same degree. The convention in the existing implementations is to
pick the first raw value that reaches or exceeds the requested temperature.

## What is still unknown after reading everything

These are the items that no source answers and that decide the electrical design.
They are the reason the schematic is not being drawn yet.

1. **How much current the supply rail can give.** Still no number. The original
   panel runs from it through a linear regulator on a heatsink, so the rail was
   built to source at least a panel's worth of current through a 17 V drop — that
   is a good prior and not a measurement. It decides the bulk capacitor and it is
   the last thing standing between this design and a schematic.
2. **Whether the rail is really isolated from mains.** Claim 3. The original panel
   is not isolated either, which says what has hung on this wall for two decades
   but does not say what is safe to connect a laptop to.
3. **The range, not the centre.** 22 V measured, with the load state unrecorded.
   The regulator's input range needs the no-load and full-load ends.
4. **Whether the machine terminates and biases the bus, and whether the original
   panel did.** The replacement should present the same bus as the panel did, so
   this is a question about the original as much as about the machine. Both
   termination and bias are footprinted and unfitted until it is answered.
5. **The real inter-frame timing.** How long the gap between frames is, how fast
   the machine answers a poll, and therefore how long the driver may stay enabled
   after the last stop bit.
6. **What the factory panel's traffic looks like.** Now for a different reason
   than before: this device has to take over that conversation, so the capture is
   the specification for what it must say and how often.
7. **Which temperature register set this machine uses** — 0x32…0x35 or 0x58…0x5C.
8. **Which registers this machine actually answers**, since the replacement has to
   provide every setting the panel could reach.
