# Target device

The device this controller talks to, and the panel it replaces or joins.

## The machine

Vallox has built residential heat-recovery ventilation units in Finland since the
1970s. The generation relevant here is the **SE series** — units whose control
electronics use the Vallox DIGIT RS-485 bus and an external wall panel as the user
interface. The units outlive their panels: the mechanical parts and the heat
exchanger last decades, while the panel's membrane keypad, its display and its
electrolytic capacitors do not, and replacement panels for the older models are no
longer manufactured.

### Models reported to speak this bus

Compiled from the three implementations that list verified hardware. This list is
**reported, not verified by this project** — every entry is `reverse-engineered`
confidence until a capture from that model exists.

| Model | Reported by |
|---|---|
| Vallox Digit SE, Digit SE 2 | kotope, Tom-Bom-badil |
| Vallox Digit SE 3500 SE (2001, LED panel) | pvainio |
| Vallox 080 / 090 / 096 / 110 / 121 / 130 D / 140 / 145 / 150 / 180 / 270 SE | kotope, Tom-Bom-badil, Creating Smart Home |
| Vallox ValloPlus 350 / 500 / 510 / 910 SE | kotope, Tom-Bom-badil |
| Helios KWL EC / ET, EC 200/300/500 Pro | FHEM wiki, Tom-Bom-badil |

Two qualifications that matter for the scope of this project:

- **Vallox 130 D needs different register numbers.** Tom-Bom-badil states this
  explicitly. Whatever this project claims to support has to be narrower than
  "Vallox".
- **Vallox MV units are a different product.** They use Modbus and an Ethernet
  interface and are out of scope. This board is for the legacy bus only.

### The unit this project is developed against

| | |
|---|---|
| Model | not determined |
| Year of manufacture | not determined |
| Serial number | not determined |
| Existing panel model | not determined |
| Number of panels on the bus | not determined |
| CO₂ sensors fitted | not determined |
| %RH sensors fitted | not determined |
| Fan type | not determined — AC (transformer-tapped) or DC (0xB0/0xB1 present) |
| Heater type | not determined — electric or water coil |

Filling this table is the first task of the next session and it needs physical
access: the model plate, a photograph of the connection box, and a photograph of
the panel's terminal block with the wires still attached.

The table is not decoration. The fan type decides whether registers 0xB0 and 0xB1
exist; the heater type decides whether the pre-heat and water-coil frost registers
mean anything; the panel count decides which bus address this board may take.

## The panel interface

From the Vallox Digit2 SE manual, section *Ohjainpaneelin asennus, irroitus ja
johdotus*. This is a manufacturer source and is quoted here because everything in
the electrical design hangs off it.

| Terminal | Wire in NOMAK cable | Signal |
|---|---|---|
| 1 | orange 1 | **+** — approx. 21 VDC |
| 2 | white 1 | **–** — return for the 21 V |
| 3 | orange 2 | **A** — RS-485 |
| 4 | white 2 | **B** — RS-485 |
| 5 | metal screen | **M** — signal ground |

Cable specified by the manufacturer: NOMAK 2 × 2 × 0.5 mm² + 0.5 mm², i.e. two
twisted pairs plus a screen. The power pair and the data pair are separate twisted
pairs; the screen is a functional conductor here, not just a shield, and it carries
the signal reference.

The manual prints one warning in bold next to this table:

> HUOM! (+) johdon virheellinen kytkentä tuhoaa ohjainpaneelin!
> *(Incorrect connection of the (+) wire destroys the control panel.)*

That sentence is a design requirement, not a caution. A replacement panel that dies
the same way is not an improvement, so reverse-polarity survival on the 21 V input
is a hard requirement for this board and is the first thing that will be tested.

### Panel addressing

Up to three panels may share the bus. The manual's procedure is to connect them one
at a time and set addresses downward from 3, ending with the last panel at address 1.
Two panels with the same address put the bus into `väylävika` (bus fault) state.

This board therefore has to be configurable to an address that is free, and the
firmware has to default to something that cannot collide with a factory panel.

### Where the 21 V comes from

The internal wiring diagram in the same manual labels the supply transformer
`M = Säästömuuntaja suojajännitekäämillä` — an autotransformer with a
protective-voltage (safety extra-low voltage) winding. The autotransformer is what
gives the SE series its eight discrete fan speeds: the AC fan motors are fed from
tapped windings, which is exactly why fan speed on this bus is a bit-mask
`0x01, 0x03, 0x07 … 0xFF` rather than a number.

The reading that matters for safety is the second half of the label: the 21 V panel
supply is stated to come from a separate protective-voltage winding, not from the
autotransformer's mains-referenced part. **That reading is from a low-resolution
scan of a wiring diagram and it must be confirmed by measurement before anything
is connected**, because if it is wrong, the panel bus is mains-referenced and the
entire design changes.

## What the replacement must do

Must:

- Read the four NTC temperatures, fan speed, and the status flags the panel shows.
- Set fan speed, and set the four writable state bits of register 0xA3.
- Sit on the bus without provoking a bus fault, alongside a factory panel if one
  is present.
- Take its own power from the bus, with no separate supply.
- Survive reversed 21 V wiring and a swapped A/B pair.

Should:

- Read RH and CO₂ if the unit has those sensors.
- Read and set the temperature setpoints (heating, bypass, defrost, pre-heat).
- Trigger the fireplace/boost function and report its remaining time.
- Report the filter service counter.

Will not:

- Replace the panel's local user interface. This is a bus client with a network
  interface, not a wall panel with buttons — the factory panel can stay.
- Touch anything on the mains side of the machine.
- Claim support for Vallox MV, or for Vallox 130 D, without a capture from one.

## What the user ends up with

A small enclosure inside or next to the machine's connection box, wired to the
panel terminal block with the manufacturer's own cable type, appearing in Home
Assistant as a climate entity with temperatures, fan speed, humidity and service
state — with no manufacturer cloud service involved and no gateway hardware.
