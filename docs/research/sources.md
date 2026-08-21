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
