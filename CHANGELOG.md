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
  write allow-list, and a bus survey that picks a free panel address by listening
  instead of by convention. 897 host-side assertions, no hardware required.
- ESPHome: wrapper rebuilt on the shared codec, listening only.

### Fixed
- The worked example in the circulated protocol documentation states the checksum
  of `01 21 11 00 A3` as `C9`. It is `D6`. Found by a unit test.

### Compatibility

| Release | Hardware | Firmware | Enclosure |
|---|---|---|---|
| — | — | — | — |
