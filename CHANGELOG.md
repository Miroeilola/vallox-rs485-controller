# Changelog

All notable changes to this project are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Hardware, firmware and enclosure are versioned separately; each release states which
combination is known to work together.

## [Unreleased]

### Added
- Initial project structure.
- Research: Vallox DIGIT RS-485 protocol and register map compiled from the
  manufacturer's Digit2 SE manual and four independent implementations, with a
  confidence rating on every claim.
- Research: panel connector pinout and wire colours from the manufacturer's manual,
  measurement plan M0-M6, risk register, and component candidates costed against
  live JLCPCB stock.
- Firmware: `vallox_protocol` codec — frame encode and decode, resynchronising
  byte-stream parser, NTC temperature table, fan speed and humidity encodings, a
  write allow-list, and a bus survey. 1420 host-side assertions, no hardware
  required.
- ESPHome: wrapper rebuilt on the shared codec, listening only.
- Tools: `scripts/vlx-decode`, a capture analyser built from the same codec as
  the firmware. Decodes telegrams into engineering units and reports the address
  and register census, which temperature register set a machine uses, and who is
  on the panel side of the bus.
- Security position written before the firmware: threat model, OTA and rollback,
  signed images, and the reasoning for leaving secure boot and flash encryption
  off in the published build.
- First measurement report: the target panel identified as a Vallox DIGIT SE LED
  panel with photographs, its terminal block, its MAX487 transceiver and its
  linear supply, and 22 V measured on the supply pair.
- Local interface analysis: LED bar against mono OLED against colour TFT, costed
  against live stock, with the power arithmetic, the OLED burn-in question, the
  legibility arithmetic and a recommendation. Lands on a 2.0" 320x240 colour IPS,
  three buttons and three indicator LEDs. A 0.96" OLED was the first answer and was
  revised: it shows eight characters by three lines at arm's length, which is a
  readout rather than the menu the display was chosen to provide, and colour TFT
  costs three to five times less per square millimetre than mono OLED.
- Buttons chosen rather than carried: four tactile switches on a resistor ladder
  into one ADC pin, because the display, bus and indicators leave ESP32-C3-MINI-1
  with thirteen usable pins and four discrete button GPIOs do not fit. Capacitive
  touch rejected (C3 has no touch peripheral, and a no-feedback control should not
  be the only way to run a household's ventilation); rotary encoder rejected on
  cost and wear.
- Measurement M2a: put an ammeter in series with the original panel's supply wire
  before removing it. Because that panel's regulator is linear, its input current
  is the rail's demonstrated capability — the cheapest available answer to the
  project's central open question.

### Changed
- **Scope: this replaces the original panel instead of joining the bus alongside
  it.** Testing on the target machine shows two controllers override each other,
  so coexistence is not available. The device is now load bearing, everything the
  panel could set has to be settable here, and whether it needs local controls is
  an open design decision.
- Transceiver chosen against the part it replaces: the original panel uses a
  MAX487, 1/4 unit load and slew-rate limited, so the replacement uses a 1/8 unit
  load slew-limited part with integrated fail-safe rather than the cheapest one.
- The bus survey in the firmware verifies that the panel side is silent before
  transmitting, instead of picking a free address.
- Hardware CI skips ERC and DRC until a KiCad project exists, instead of failing.

### Fixed
- The worked example in the circulated protocol documentation states the checksum
  of `01 21 11 00 A3` as `C9`. It is `D6`. Found by a unit test.

### Compatibility

| Release | Hardware | Firmware | Enclosure |
|---|---|---|---|
| — | — | — | — |
