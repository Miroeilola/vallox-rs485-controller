# Sources

Every source used in this research, with the date it was read and how far it can be
trusted. Ratings are defined in [README.md](README.md).

Nothing here is copied into this repository. Manufacturer documents are linked and
quoted only where a quotation is needed to make a claim checkable.

## Manufacturer documents

| Source | Read | What it gave | Rating |
|---|---|---|---|
| Vallox Digit2 SE / VKL / MLV installation, operation and maintenance manual (Finnish), distributed by netrauta.fi — [PDF](https://www.netrauta.fi/attachments/ohjeita/vallox/vallox_digit2_se.pdf) | 2026-08-21 | Panel terminal pinout `1..5 = + – A B M`, wire colours, "n. 21 VDC" panel supply, cable type NOMAK 2×2×0.5 mm² + 0.5 mm², maximum 3 panels / 5 CO₂ sensors / 2 RH sensors, panel address assignment procedure, internal wiring block diagram naming the supply transformer. **Re-read 2026-08-22:** the file is a vector drawing (FreeHand MX → Distiller 7), not a scan. Page 7 *Sisäinen sähkökaavio* rendered at 600 dpi shows one transformer `M` with the tapped 230/180/160/140/120/100/80/60 V fan winding and a **separate secondary drawn across the core bars**, entering the mainboard through its own 2-pin connector next to `SULAKE T800mA`; the mainboard's low-voltage header carries `GND, −, +, A, B` as separate pins and no earth symbol; `PM = Peltimoottori 21 VDC` | `manufacturer, read` |
| Vallox Digit SE / SE VKL tekninen ohje 12/99 (print 1.09.61F/05.99) — the LED-panel generation — [PDF](https://ilmastointitohtorit.fi/wp-content/uploads/2025/01/Digitset.pdf), mirror at [sisailmahuolto.com](https://sisailmahuolto.com/wp-content/uploads/2013/09/Digitset.pdf) | 2026-08-22 | p. 6 *Sisäinen sähkökaavio* (vector, FreeHand 8): legend `M1 Säästömuuntaja suojajännitekäämillä`, **`M2 Muuntaja 230VAC/16VAC`**, `M Peltimoottori 16 VAC`; the drawing shows M2 as a two-winding transformer whose 16 V secondary enters the mainboard through the `SULAKE T800mA` holder; `max. 7 kpl ohjainpaneeleita`, 5 CO₂, 2 RH; terminal pinout `A B − + M`, `n. 21 VDC` | `manufacturer, read` |
| Vallox 121 SE tekninen ohje (TEKN121SE_SF_250111, 2011) — [PDF](https://www.taloon.com/media/attachments/Vallox_ilmanvaihtokone_121_SE_Tekninen_ohje.pdf) | 2026-08-22 | legend `M1 Säästömuuntaja suojajännitekäämillä 230VAC/16VAC`; `n. 21 VDC` panel supply; `5 = metalli = signaalimaa` | `manufacturer, read` (text; diagram page inspected in the research pass) |
| Vallox ValloPlus 350 SE Betriebsanleitung 2015-03 (Vallox GmbH) — [PDF](https://vallox.de/wp-content/uploads/2023/09/BA_ValloPlus-350SE_2015-03.pdf) | 2026-08-22 | `M1 Schutzspannungstransformator 230 VAC/16 VAC`; spare part 16 `Schutzspannungstransformator 940150`; `Glaspatronensicherung im Gerät T800 mA` | `manufacturer, read` |
| Vallox Digit SE technical guide, English (TEKNdigitSE_E_280808), the only Vallox-hosted copy found — [PDF](https://res.cloudinary.com/vallox/image/upload/v1694083723/FileStock/ValidManuals/TEKNdigitSE_E_280808.pdf) | 2026-08-22 | `= + ca. 21 VDC`, `max. 3 control panels` | `manufacturer, read` |
| Vallox CO₂ sensor for Digit SE, tuotekortti 1.09.375F/2013 — [PDF](https://www.taloon.com/media/attachments/vallox/hiilidioksidianturi_vallox_digit_se_180-280.pdf) | 2026-08-22 | `Syöttöjännite: n. 21 VDC ilmanvaihtokoneelta`, RS-485; **no current figure** — none of Vallox's bus accessories publishes one | `manufacturer, read` |
| Spare part *Vallox ohjauksen muuntaja 230V/16V/14VA*, part 940130, listed for Digit SE (3500 SE), 90 SE, 121 SE, 200 SE — [Huolto Vuorio](https://www.huoltovuorio.fi/en/product/vallox-transformer-for-230v-16v-14va-control/), [Suodatinkeskus](https://www.suodatinkeskus.com/fi/product/vallox-ilmava-digit-digit-s-muuntaja-230v-16v-14va/1163) | 2026-08-22 | the control transformer's VA rating: **14 VA** | `manufacturer, quoted` (reseller listings of the OEM part) |
| Vallox LON–RS485 gateway technical note (Finnish), referenced via docplayer | 2026-08-21 | Gateway operating voltage 21 VDC taken from the ventilation unit; 9600 bps, N, 8, 1 on a shielded twisted pair | `manufacturer` |
| Vallox DIGIT väyläprotokolla (Finnish protocol description) and its English translation `Digit_protocol_english_RS485.pdf` — [PDF on the FHEM wiki](https://wiki.fhem.de/w/images/7/7e/Digit_protocol_english_RS485.pdf), "Translated April 11 2021 from the Finnish document written by Vallox/Petteri Kähärä 27.06.2011", 10 pp. | 2026-08-21 (not reachable), **read 2026-08-22** | Requester "waits for a response for a maximum of 10 ms", resends, and "if 10 responses are not received … enters fault mode"; a *send/acknowledge* service in which the receiver "acknowledges the checksum of the received packet", and an *unacknowledged* service; `8FH TRANSMISSION ALLOWED ONLY IN WRITING`, `91H TRANSMISSION PROHIBITED`; Annex B (the translator's own Helios capture, terminal ID 0x2E): the master "every 12 seconds broadcasts the important registers" and the user terminal "requests data for its display every 6 seconds approx." | `manufacturer, read` (translation; the Finnish original is still unobtained, and Annex B is the translator's capture, not Vallox text) |

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

## Captures and reports published by others

Read 2026-08-22 while looking for what the factory panel says on the bus (M3) and
what the +/− pair has powered (M2). A capture is first-hand evidence about *that*
installation; it is not a measurement on the target machine.

| Source | What it gave | Rating |
|---|---|---|
| [openHAB community — "Vallox SE Binding"](https://community.openhab.org/t/vallox-se-binding/86425) (110 SE R with an FBD 382 LCD panel; also a 90 SE) | The only **timestamped** log found: panel 0x21 poll group every ≈ 5.15 s (`01-21-11-00-A3-D6` → `01-11-21-A3-01-D7`, then `00-29`, `00-35`, `00-A3`, `00-71`), mainboard broadcast to 0x20 of `2B, 2C, 35, 34, 32, 33` at ≈ 130 ms spacing and `2A` ≈ 2.4 s later; reply latency 3–130 ms as seen by the logger | `first-hand capture` |
| [Home Assistant community — "Serial communication and templates RS-485"](https://community.home-assistant.io/t/serial-communication-and-templates-rs-485/194528) | Same broadcast block and the same repeating poll set, no timestamps | `first-hand capture` |
| [creatingsmarthome.com part 1, comments](https://www.creatingsmarthome.com/index.php/2020/08/25/guide-vallox-digit-ventilation-to-home-assistant-part-1-2-hardware/) (121 SE, Digit2 SE, 110/140/180 SE; comments through 2026-01) | `01 21 11 00 a3 d6` / `01 11 21 a3 01 d7` / `01 21 11 00 71 a4`; ESP-01 + MAX485 module fed from the "24 V" pair through an adjustable buck, several confirmed builds, no brown-out or bus-fault report, one reversed-polarity loss | `first-hand capture`, `first-hand community` |
| [lampopumput.info — "Vallox Digit väylä"](https://lampopumput.info/foorumi/threads/vallox-digit-v%C3%A4yl%C3%A4.14203/) (Digit SE, 2014) | Unsolicited mainboard frames `01 11 20 49 00 7B`, `4A`, `4C`, `53`, `54`, `5B`, `5C`, `5A`, `58` — registers not in the protocol document; "emokortti keskustelee kauko-ohjaimelle säännöllisesti (10 s)" | `first-hand capture` |
| [lampopumput.info — "Vallox Digit SE väylävika"](https://lampopumput.info/foorumi/threads/vallox-digit-se-v%C3%A4yl%C3%A4vika.36707/) | Rail measured 23.7 V in a bus-fault case, called "aivan normaali" by a Vallox-trained poster; fault traced to the mainboard | `anecdotal` |
| [lampopumput.info thread 31004](https://lampopumput.info/foorumi/threads/vallox-digit-se-vaihtaa-itsekseen-peruspuhallinnopeuden-suuremmaksi.31004/) (SED panel, 2021) | "Jos sen kytkee irti, konetta ei taida saada käyntiin lainkaan" — one voice on what happens with no panel on the bus | `anecdotal` |
| [MySensors forum — windkh's Digit SE thread](https://forum.mysensors.org/topic/949/) | Decoded frames: `21→11 cmd=0 arg=A3`, `00/29`, `00/35`, `00/71`, `00/A3` | `first-hand capture` (decoded) |
| [Tom-Bom-badil wiki — Protocol basics](https://github.com/Tom-Bom-badil/home-assistant_helios-vallox/wiki/Protocol-basics-(simplified)) and [Tested adaptors](https://github.com/Tom-Bom-badil/home-assistant_helios-vallox/wiki/Appendix-1-%E2%80%90-Tested-adaptors) | "Each remote control polls the mainboard every ~5.5 seconds"; mainboard "broadcasts … approximately every 12 seconds"; bus-powered RS-485/Ethernet adapters in use (Waveshare RS485 TO POE ETH (B); USR DR-134, 5–24 V in, [97 mA @ 5 V per its datasheet](https://www.pusr.com/uploads/20240416/7ac1d1cf166f027f4001075ae117e270.pdf)) | `reverse-engineered`, `first-hand community` |
| [lostcontrol/esphome-helios-kwl](https://github.com/lostcontrol/esphome-helios-kwl) (Helios KWL EC 500 R, same bus) | "I'm powering an ESP32 from the KWL's 24V using a BEC" | `first-hand community` |

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
