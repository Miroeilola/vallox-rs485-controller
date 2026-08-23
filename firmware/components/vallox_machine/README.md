# vallox_machine — a simulated Vallox mainboard

An emulator of the machine side of the RS-485 bus, used by the host tests and
by the browser simulator. It is itself a bus device at 0x11 and speaks only in
the six-byte frames of `vallox_protocol`. **It is exactly as right as
`docs/research/protocol.md`** — nothing here has been checked against a
machine. When the M3 capture disagrees, the protocol document is corrected
first, then this model and the panel in the same PR.

## What it does

| Behaviour | Source | Confidence |
|---|---|---|
| Answers a poll for a known register with its value, to the poller | protocol.md claims 11 (reverse-engineered), 24 (manufacturer) | implementations / manufacturer |
| Answers a write with one byte, the received checksum | claim 25 | manufacturer |
| Ignores writes to read-only or unknown registers, without a reply | no NAK is documented | assumed (silence) |
| Broadcasts `2B 2C 35 34 32 33 2A` to 0x20 every 12 s, 130 ms apart | claim 23 | manufacturer (period) / reverse-engineered (contents) |
| Register map and defaults (status defaults to power on + winter mode / heat recovery) | `vallox_machine_regs.h`, each row with its class | implementations |
| Thermal model: lags toward indoor / recovered / setpoint temperatures | own model, parameters | n/a — a model, not a claim |
| Frost protection: supply fan stop when exhaust drops below the 0xA8 threshold (NTC table, a writable setting), output is bit 3 of 0x08 (IO_MULTI_2); hysteresis from 0xB2 | threshold 0xA8, output 0x08 bit 3, hysteresis 0xB2 | implementations |
| Fault injection; cleared by host or by the panel writing 0 to 0x36 | clearing write | **assumed** |
| Poll-answer delay 0 / 10 / 200 ms / never, counted from the tick that hears the frame; a never-answering machine also withholds the acknowledge | test knob | n/a |

## What it does not do (yet)

- No CO₂ / RH sensors (`0x2D` reports none fitted), no SUSPEND/RESUME exchange.
- No LON module, no programme-2 week clock, no start-up burst from the mainboard
  (undocumented).
- No legacy temperature registers 0x58–0x5C (which set a machine uses is discovered
  from traffic, not assumed).
- Broadcast writes to 0x10 are applied like unicast writes — assumed.
- The model does not poll the panel. Whether the real mainboard does is an M3 question.
- Frost protection raises no fault code: none is documented for exchanger frost (0x09 is
  the water-coil fault); it only sets the supply-fan bit of 0x08 (IO_MULTI_2).
- The service (bit 7) and filter (bit 4) indicator bits of 0xA3 are never set; the month
  counter 0xAB decrements but the reminder indicator is not modelled.

## Guard

`scripts/check-machine-regs.py` (run by CI) cross-checks `vallox_machine_regs.h`
against `docs/research/protocol.md`: writability per section, NTC-encoded
registers in the degree table, and registers the document does not describe.
It currently warns about 0x6C and 0x6E (flags 1 and 3): they are in
`vallox_protocol.h` but protocol.md has no row for them, so the model answers
them with 0 — a documented gap, not a claim.

## Using it

```c
vlx_machine_t m; vlx_machine_init(&m);
vlx_machine_feed(&m, rx, n);                    // bytes heard on the bus
n = vlx_machine_tick(&m, now_ms, tx, sizeof tx); // send these
m.p.t_outdoor = -15.0f; m.p.time_scale = 60.0f; // side panel knobs
vlx_machine_fault(&m, VLX_FAULT_SUPPLY_AIR_SENSOR);
```

Tests: `firmware/test/host/test_vallox_machine.c` and `test_e2e_membus.c`.
