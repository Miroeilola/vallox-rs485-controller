# Measurements

Every claim about this device is backed by a measurement recorded here. One file
per question, named `YYYY-MM-DD-<topic>.md`.

The plan that produces the reports is
[`../research/measurement-plan.md`](../research/measurement-plan.md).

| Report | Answers |
|---|---|
| [2026-08-21-panel-identification.md](2026-08-21-panel-identification.md) | M0 (partly) and the supply-voltage row of M1: which panel, what is on its terminals, 22 V on the pair |
| [2026-08-22-panel-current.md](2026-08-22-panel-current.md) | M2a: the factory panel draws 450 mA continuous, 700 mA momentary — conditions still to be recorded |
| [2026-08-22-rail-voltage-no-load.md](2026-08-22-rail-voltage-no-load.md) | M1, no-load row: 22.8 V with the panel disconnected; the buck stays TPS54202 |

## Instruments used

The instrument matters: a reader judges a ripple figure differently depending on
whether it came from a 20 MHz handheld scope or a bench instrument with proper
probing. Model numbers go in as each instrument is first used, not later.

| Instrument | Model | Used for |
|---|---|---|
| Multimeter, CAT III | not yet recorded — used for the 22 V and the 450/700 mA readings, model to be added | M1 — is the panel bus safe to touch, and what is on it; M2a — panel current |
| Oscilloscope | not yet recorded | M1 rail ripple, M3 bus waveform, M4 frame timing |
| Differential or isolated probe | not yet recorded | M1, only if the rail turns out not to be isolated |
| Electronic load or resistor decade | not yet recorded | M2 — how much current the 21 V rail can give |
| USB RS-485 adapter, transmit disabled | not yet recorded | M3 — passive capture |
| Logic analyser | not yet recorded | M3, M4 — byte-level timing alongside the scope |
| Reference thermometer | not yet recorded | Checking the NTC table against reality |
| Infrared thermometer or thermocouple | not yet recorded | Temperature under load, enclosure closed |

The RS-485 adapter is the one instrument whose behaviour has to be verified
before it is used: it must be incapable of transmitting. That check happens on
the bench against a second adapter and it is part of the M3 report, not a
precondition anyone is trusted to remember.

## Required before release

- [ ] Current consumption in every operating mode
- [ ] Rail voltages and ripple under load
- [ ] Start-up behaviour: rise time, sequencing, inrush
- [ ] Interface signal integrity and error rate over a long run
- [ ] Temperature under load, enclosure closed
- [ ] Wireless range with the enclosure in place
- [ ] 72 h continuous run without a reset
- [ ] Reverse-polarity survival on the 21 V input, and 21 V applied to a data line

The last item is not a general requirement, it is specific to this device: the
manufacturer states that miswiring the positive conductor destroys the factory
panel, and surviving that is a design claim this project makes. A claim of that
kind needs a bench test with a photograph of the setup and the board working
afterwards.
