# Sources

Every source used in this research, with the date it was read and how far it can be
trusted. Ratings are defined in [README.md](README.md).

Nothing here is copied into this repository. Manufacturer documents are linked and
quoted only where a quotation is needed to make a claim checkable.

## Manufacturer documents

| Source | Read | What it gave | Rating |
|---|---|---|---|
| Vallox Digit2 SE / VKL / MLV installation, operation and maintenance manual (Finnish), distributed by netrauta.fi — [PDF](https://www.netrauta.fi/attachments/ohjeita/vallox/vallox_digit2_se.pdf) | 2026-08-21 | Panel terminal pinout `1..5 = + – A B M`, wire colours, "n. 21 VDC" panel supply, cable type NOMAK 2×2×0.5 mm² + 0.5 mm², maximum 3 panels / 5 CO₂ sensors / 2 RH sensors, panel address assignment procedure, internal wiring block diagram naming the supply transformer | `manufacturer` |
| Vallox LON–RS485 gateway technical note (Finnish), referenced via docplayer | 2026-08-21 | Gateway operating voltage 21 VDC taken from the ventilation unit; 9600 bps, N, 8, 1 on a shielded twisted pair | `manufacturer` |
| Vallox DIGIT väyläprotokolla (Finnish protocol description) and its English translation `Digit_protocol_english_RS485.pdf`, circulated via LoxWiki and FHEM | 2026-08-21 | Not obtained directly — `docplayer.fi` and `loxwiki.eu` were unreachable from this machine. The register semantics below match this document as quoted by three independent implementations, so the document is treated as the upstream origin but is **not** counted as a source that was read. | not read |

## First-hand, from the machine itself

The strongest sources in this project are not documents.

| Source | Date | What it gave | Rating |
|---|---|---|---|
| The target panel, removed and photographed | 2026-08-21 | Panel identified as a Vallox DIGIT SE LED panel; terminal block `+ − A B M` confirmed on the hardware; installed wire colours, which do not match the manufacturer's colour code; MAX487 transceiver read off the board; linear supply on a heatsink. Report: [`../measurements/2026-08-21-panel-identification.md`](../measurements/2026-08-21-panel-identification.md) | `first-hand` |
| Supply voltage at the terminal | 2026-08-21 | 22 V DC. Instrument and load state not recorded, so the figure confirms the order of magnitude and nothing more | `first-hand, conditions incomplete` |
| Prior testing on the target machine | before 2026-08-21 | Two controllers on this bus override each other. This is why the project replaces the panel rather than joining the bus, and it contradicts the manufacturer's "up to three panels" for any client that is not a Vallox panel | `first-hand` |
| Analog Devices MAX481/483/485/487–491/MAX1487 datasheet | 2026-08-21 | MAX487: 48 kΩ receiver input, 1/4 unit load, up to 128 nodes, slew-rate limited to 250 kbps — the characteristics the replacement has to match | `manufacturer, quoted` |
| Texas Instruments THVD1400 / THVD1420 datasheet, SLLSF78B, Dec 2020 rev. Oct 2021 | 2026-08-21 | Candidate transceiver: 1/8 unit load, integrated open/short/idle fail-safe, 500 kbps slew-limited, bus pins −16 V to +16 V absolute maximum, ±12 kV IEC contact ESD, 700–900 µA quiescent receiving | `manufacturer, read` |

**`manufacturer, quoted` versus `manufacturer, read`.** The TI datasheet was
retrieved and its tables read directly. The Analog Devices one was not: their
server fails HTTP/2 negotiation and times out on HTTP/1.1 from here, so the MAX487
figures come from a search result quoting that datasheet rather than from the
document. The numbers are unambiguous and the part is thirty years old, but the
distinction is the whole method of this file and it would be hypocritical to skip
it here. Re-read the PDF and upgrade the rating before any of it reaches a
datasheet of ours.

## Independent implementations

These are separate code bases by separate authors. Where they agree on a register
value, the agreement is evidence; where only one of them states something, it is not.

| Source | Read | What it gave | Rating |
|---|---|---|---|
| [windkh/valloxserial](https://github.com/windkh/valloxserial) (`library/ValloxProtocol.h`) | 2026-08-21 | The most complete annotated register map: telegram layout with a worked example, address ranges including LON = 0x28, bit-level meaning of the flag registers 0x06–0x08 and 0x6C–0x71, SUSPEND/RESUME, full 256-entry NTC table | `reverse-engineered` |
| [pvainio/vallox-rs485](https://github.com/pvainio/vallox-rs485) (`vallox.go`) | 2026-08-21 | Frame struct, checksum implementation, 9600 8N1, query encoding, fan-speed table, identical NTC table, 100 ms bus-idle rule before transmitting | `reverse-engineered` |
| [kotope/valloxesp](https://github.com/kotope/valloxesp) (`vallox_protocol.h`, `Vallox.h`) | 2026-08-21 | Register subset with status-flag bit names, fireplace/boost handling, poll byte 0x00, a list of 11 Vallox models the author reports as verified | `reverse-engineered` |
| [pecca/vallox_control](https://github.com/pecca/vallox_control) (`c/digit_protocol.c`) | 2026-08-21 | Independent C implementation; message field indices, register set including DC fan adjustments 0xB0/0xB1 | `reverse-engineered` |
| [mld18/ioBroker.valloxSerial](https://github.com/mld18/ioBroker.valloxSerial) | 2026-08-21 | Checksum stated in words ("add all bytes, the low byte is the checksum"), five-wire bus table with the same wire colours as the Vallox manual, panel addresses 1–9 | `reverse-engineered` |
| [Tom-Bom-badil/home-assistant_helios-vallox](https://github.com/Tom-Bom-badil/home-assistant_helios-vallox) | 2026-08-21 | Register table with read/write flags per bit, defrost hysteresis encoding, fault-code table, confirmation that Helios EC/ET Pro units speak the same bus, statement that NTC sensors are 5 kΩ type | `reverse-engineered` |

## Guides and community reports

| Source | Read | What it gave | Rating |
|---|---|---|---|
| [Creating Smart Home — Vallox Digit to Home Assistant, part 1 (hardware)](https://www.creatingsmarthome.com/index.php/2020/08/25/guide-vallox-digit-ventilation-to-home-assistant-part-1-2-hardware/) | 2026-08-21 | Practical wiring against a real unit; reports 24 V at the machine terminal rather than 21 V; reports that RS-485 modules without automatic direction control cause a `Väylävika` (bus fault) on the Vallox display | `anecdotal` |
| [FHEM wiki — Vallox](https://wiki.fhem.de/wiki/Vallox) | 2026-08-21 | Confirms Helios KWL EC/ET compatibility with the same bus | `anecdotal` |

## What existing work already does, and what this project adds

All of the implementations above solve the software problem, and several solve it
well. None of them ships hardware: the usual build is a development board, an
RS-485 breakout and a step-down module on a piece of DIN rail or loose in the
machine's cable box.

This project is therefore not attempting to re-derive the protocol. It credits the
work above, verifies it by measurement, and adds the part that is missing:

1. A single board that takes the panel connector directly, survives the wiring
   mistake the manufacturer warns about, and fits an enclosure.
2. A measured protocol description — oscilloscope captures of the frame, bus levels
   and timing — rather than a register map that has to be read out of source code.
3. Published measurements: current draw, rail quality, bus error rate over a long
   run, temperature in the enclosure.
4. A datasheet and an enclosure, so the result is a device rather than a project.
