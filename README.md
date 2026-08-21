# Vallox RS-485 Controller

Replacement bus controller for legacy Vallox ventilation units.

<!-- Add a photo of the finished device here. A real photograph, not a render. -->

[![hardware](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/hardware.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/hardware.yml)
[![firmware](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/firmware.yml/badge.svg)](https://github.com/Miroeilola/vallox-rs485-controller/actions/workflows/firmware.yml)
![hardware revision](https://img.shields.io/badge/hardware-rev%20A-blue)
![license](https://img.shields.io/badge/license-MIT%20%2F%20CERN--OHL--S%20%2F%20CC--BY--SA-green)

## What this replaces

<!-- The concrete problem. What device, what limitation, why a replacement is worth building. -->

## Specifications

| | |
|---|---|
| Microcontroller | ESP32-S3 |
| Supply voltage | — |
| Current consumption | — (measured, see `docs/measurements/`) |
| Interfaces | — |
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

<!-- Honest limitations. What is untested, what is out of scope, what is known to be
     imperfect. This section is required and must not be empty — it is the most
     credible part of the document. -->

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
