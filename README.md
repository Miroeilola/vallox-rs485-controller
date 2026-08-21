# Vallox RS-485 Controller

Replacement bus controller for legacy Vallox ventilation units.

<!-- Add a photo of the finished device here. A real photograph, not a render. -->

[![hardware](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/hardware.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/hardware.yml)
[![firmware](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/firmware.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/firmware.yml)
![hardware revision](https://img.shields.io/badge/hardware-rev%20A-blue)
![license](https://img.shields.io/badge/license-MIT%20%2F%20CERN--OHL--S%20%2F%20CC--BY--SA-green)

> **Status: research.** No board exists yet. The bus protocol has been compiled from
> the manufacturer's manual and four independent implementations, and the parts have
> been costed, but nothing has been measured on a machine. What is known, what is
> only reported, and what has to be measured before a schematic is drawn is written
> down in [`docs/research/`](docs/research/).

## What this replaces

Vallox has built residential heat-recovery ventilation units in Finland since the
1970s, and the SE-series machines are still in service in tens of thousands of
houses. The machine outlives its control panel: the mechanics and the heat exchanger
last decades, the panel's keypad and its electrolytics do not, and panels for the
older models are no longer made.

The machines already carry an RS-485 bus. The panel is a client on it, up to three
panels are allowed, and everything the panel shows — four temperatures, fan speed,
humidity, CO₂, fault and service state — travels over that bus in a six-byte
telegram. This board joins that bus as one more client, takes its power from the
same five terminals the panel uses, and puts the machine into Home Assistant or MQTT
with no manufacturer cloud service and no gateway hardware.

**The factory panel stays.** This is not a panel replacement. If this board fails,
the machine keeps running and the household keeps its buttons.

### This is not the first attempt, and that is worth saying

Several people have solved the software side, and solved it well —
[pvainio](https://github.com/pvainio/vallox-rs485),
[kotope](https://github.com/kotope/valloxesp),
[windkh](https://github.com/windkh/valloxserial),
[pecca](https://github.com/pecca/vallox_control),
[mld18](https://github.com/mld18/ioBroker.valloxSerial) and
[Tom-Bom-badil](https://github.com/Tom-Bom-badil/home-assistant_helios-vallox).
The register map here comes from reading their work and is credited in
[`docs/research/sources.md`](docs/research/sources.md).

What none of them ships is hardware. The usual build is a development board, an
RS-485 breakout and a step-down module, loose inside the machine's cable box. This
project adds the missing half: one board that lands on the panel terminals, survives
the wiring mistake the manufacturer warns about in bold, a measured protocol
description with oscilloscope captures rather than a register map you have to read
out of source code, published measurements, an enclosure and a datasheet.

## Specifications

| | |
|---|---|
| Microcontroller | not fixed — ESP32-C3 proposed, see `hardware/docs/decisions.md` |
| Supply voltage | — (manufacturer states approx. 21 VDC at the panel terminal; range not measured) |
| Current consumption | — (measured, see `docs/measurements/`) |
| Interfaces | RS-485 half duplex, 9600 8N1, Vallox DIGIT protocol · Wi-Fi 2.4 GHz |
| Dimensions | — |
| Operating temperature | — |
| Enclosure | — |

Every number in this table comes from a measurement or a component datasheet.
Values still marked `—` have not been measured yet.

## Repository layout

| Path | Contents |
|---|---|
| `hardware/` | KiCad 10 project, manufacturing outputs, order packages |
| `firmware/` | ESP-IDF application and reusable components |
| `esphome/` | ESPHome external component (Home Assistant users start here) |
| `mechanical/` | Enclosure: STEP source, STL for printing, drawings, print profiles |
| `docs/` | Research, measurements, datasheet, images |

## Building one

### 1. Order the board

```bash
git clone --recurse-submodules https://github.com/Miroeilola/vallox-rs485-controller.git
```

Manufacturing files for the released revision are attached to the GitHub release.
The frozen package that was actually sent to the fab is in
`hardware/orders/<rev>-<date>/`, together with what it cost and how long it took.

### 2. Print the enclosure

See [`mechanical/README.md`](mechanical/README.md) for material, print settings and
hardware (screws, heat-set inserts).

### 3. Flash the firmware

```bash
cd firmware
idf.py set-target esp32s3
idf.py build flash monitor
```

Pre-built binaries are attached to each release.

### Home Assistant

See [`esphome/`](esphome/) for the external component and a working example
configuration.

## Measurements

Every claim in this repository is backed by a measurement in
[`docs/measurements/`](docs/measurements/): current consumption per operating mode,
rail voltages and ripple, bus signal integrity, and temperature under load.

## What this does not do

- **It is not a control panel.** No display, no buttons. It needs Home Assistant or
  an MQTT broker to be worth anything, and the factory panel has to stay for local
  control.
- **It does not support Vallox MV.** Those units use Modbus and have their own
  interface. This is for the legacy DIGIT RS-485 bus only.
- **It does not support Vallox 130 D** without changes — that model is reported to
  use different register numbers, and no capture from one exists here.
- **It will claim support only for machines a capture exists from.** Other SE-series
  models and Helios KWL EC/ET units are reported to use the same bus, and reported is
  not the same as verified. Which registers a given machine answers is a property of
  that machine.
- **It does not write registers that have not been verified on hardware.** The
  firmware carries an explicit allow-list, it starts with one entry, and a register
  joins it only when a measurement report exists for it. Vallox warns that writing an
  incorrect register or value can damage the unit.
- **It makes no safety or IP-rating claim.** Nothing has been tested for either.
- **Connecting anything to the machine voids the manufacturer's warranty** and is
  your decision, not this project's.

## Tools

[`scripts/vlx-decode`](scripts/vlx-decode/) decodes a raw bus capture into
annotated telegrams and a census of the bus. It shares the codec with the
firmware, so it cannot drift from what the device believes.

```bash
cd scripts/vlx-decode && make && ./vlx-decode --demo
```

## Research

Everything known before the design started, with sources and confidence ratings, is
in [`docs/research/`](docs/research/): the
[protocol](docs/research/protocol.md) and its register map, the
[target device](docs/research/target.md) and its panel pinout, the
[measurement plan](docs/research/measurement-plan.md) that has to run before a
schematic is drawn, the [risks](docs/research/risks.md), and the
[component candidates](docs/research/component-candidates.md) with live stock and
prices.

## License

| Path | License |
|---|---|
| `firmware/`, `esphome/`, `scripts/` | [MIT](LICENSES/MIT.txt) |
| `hardware/`, `mechanical/` | [CERN-OHL-S-2.0](LICENSES/CERN-OHL-S-2.0.txt) |
| `docs/`, README, images | [CC-BY-SA-4.0](LICENSES/CC-BY-SA-4.0.txt) |

See [LICENSE.md](LICENSE.md) for details.

## About

Designed and built by Miro Eilola / [Mironet](https://mironet.fi).
Mironet designs embedded devices like this one on commission — electronics,
enclosure and firmware from specification to production. miro@mironet.fi
