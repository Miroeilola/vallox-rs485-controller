# Local user interface: display or LEDs

The original panel is not on a living-room wall. It is **under the machine**, in a
technical space, and there is no size to match and nothing to cover up. That
removes the constraint that was driving this decision a day ago, and it changes
the answer.

This file compares the options with real prices and live stock, read from the
JLCPCB catalogue on **2026-08-21**. It ends with a recommendation and with the
argument against it, because the argument against it is good.

## First: what is the local interface actually for

Three jobs, and they are not equally important.

| Job | How often | What it needs |
|---|---|---|
| **Change fan speed** | daily-ish | Two buttons and a way to see the result |
| **Control when the network is down** | rarely, but it is the whole reason this exists | The same |
| **Commissioning and diagnostics** | at install, and every time something is wrong | Wi-Fi state, IP address, bus frame and error counters, firmware version |
| **Change setpoints** — heating, bypass, pre-heat, fan limits | a few times a year | Something that can show a number and a name |

The last two are where a display earns its price. The first two are where LEDs are
perfectly good and arguably better.

The fourth row is not optional. Risk R11 says everything the old panel could reach
has to be reachable here, and the old panel could set the heating setpoint from a
10…27 °C bar graph on the wall. Whether "reachable" means *locally* or *through
Home Assistant* is the real question hiding inside this decision.

## The options

### A — LED bar and buttons, like the original

| Part | LCSC | Qty | $/pc | Class |
|---|---|---|---|---|
| LED 0603, 19-217 family | C2986059 green, C2986060 red | 8 | 0.018 | extended |
| Shift register 74HC595D | C5947 | 1 | 0.130 | **basic** |
| Tactile switch 5.1 mm | C318884 | 3 | 0.020 | **basic** |
| Series resistors 0402 | — | 8 | 0.002 | basic |

**About $0.36 per board**, all of it assembled by the fab.

A faithful replica of the original — 8 fan LEDs, 8 temperature LEDs, 4 status
LEDs and eight buttons — comes to roughly $0.75. Still the cheapest option by a
wide margin.

### B — 0.96" OLED, 128 × 64

| Part | LCSC | Stock | $/pc |
|---|---|---|---|
| HS96L03W2C03, 0.96" 128×64, SSD1315, I2C, white | C5248080 | 2 467 | 2.314 |
| FPC connector, 30P 0.5 mm | C5213742 | 5 932 | 0.179 |
| Tactile switch 5.1 mm ×3 | C318884 | 1 683 297 | 0.020 |

**About $2.55 per finished device**, of which **only $0.24 is on the assembled
board**. The display is a bare glass panel with an FPC tail: the fab solders the
connector, and the display is plugged in afterwards by hand. It is not part of the
PCBA order at all.

Active area 21.7 × 10.9 mm. Rated −40…+70 °C, which matters in a space that shares
air with a ventilation machine.

Larger alternatives, both needing their own footprint rather than dropping in:
1.3" SH1106 (C7465997, $5.33) with a 29.4 × 14.7 mm active area, and 1.54"
SSD1309 (C7465999, $6.46).

### C — 1.14" colour IPS TFT, 240 × 135

| Part | LCSC | Stock | $/pc |
|---|---|---|---|
| N114-2413THBIG01-H13, ST7789V, SPI | C2890618 | 421 | 2.536 |
| FPC connector | — | — | ~0.18 |
| Backlight driver, if the panel's LEDs are in series | — | — | ~0.30 plus an inductor |

**About $3.05, and possibly more.** The open question is the backlight: a 1.14"
panel usually has two or three white LEDs in series, which needs 6–9 V and
therefore a boost converter on a board whose whole supply argument is about
efficiency. That has to be read out of the panel's own datasheet before this
option can be costed honestly. Stock is 421 pieces, which is a coincidence rather
than a supply.

### D — E-paper

**Not available.** There is no e-paper in the fab's catalogue, so it would be an
open-market purchase with its own lead time. It is also the wrong technology here
for a reason that has nothing to do with price: e-paper is reflective and needs
ambient light, and this panel lives under a machine in a room that is dark unless
somebody turns the light on. Ruled out.

## The comparison that matters

### Cost, in context

The delta between the cheapest and the nicest is about **$2.20 per device**. The
board's parts come to roughly $4.65, so a display is a 45 % increase on the parts
— and on a finished device with a PCB, assembly, an enclosure and shipping, it is
somewhere near 8 %. Cost is real but it is not decisive, and pretending otherwise
would be false precision.

The more interesting cost fact is that **the display barely touches the PCBA
order**: $0.24 of connector and switches. Whoever builds one can decide at
assembly time whether to fit a display, and the same board takes either.

### Power, which turned out not to be the obstacle

The original panel converts 22 V to its logic rail with a **linear** regulator.
A linear regulator draws its output current at its input voltage, so the panel
pulls whatever its 5 V circuitry needs *straight from the bus rail* — and the
heatsink says that is on the order of a watt.

Working backwards from the heatsink: about 1 W dissipated across a 17 V drop is
roughly **60 mA**, so the rail has been delivering about **1.3 W** to this panel
for twenty years.

Our board uses a switching converter. At 85 % efficiency, that same 1.3 W becomes
about **330 mA available at 3.3 V**. Against that budget:

| Load | Typical at 3.3 V |
|---|---|
| ESP32-C3, Wi-Fi connected, DTIM3 | 30–45 mA average |
| 0.96" OLED, typical content | 10–20 mA |
| 1.14" IPS TFT with backlight | 40–60 mA |
| 8 indicator LEDs at 2 mA | 16 mA |

**Every option fits, with room to spare.** The switching converter buys roughly a
6× advantage over the linear one it replaces, and that advantage is larger than
the entire display budget.

**This is arithmetic, not a measurement.** The 60 mA is inferred from a heatsink
in a photograph. It is replaced by a real number in M2a below, and if that number
comes back much smaller, this whole section is rewritten and option C dies first.

### Lifetime, where the LEDs win and it is not close

The panel being replaced has run its LEDs continuously since roughly 2001 and they
still light — that is visible in the photographs. Indicator LEDs at a few
milliamps outlast the equipment they are fitted to.

A PMOLED does not. Passive-matrix OLED lifetime is specified to half brightness in
the region of 10 000 to 50 000 hours *for the lit pixels*, and 10 000 hours is
fourteen months of continuous operation. A display showing a static fan speed on a
device that never sleeps will burn that image in, and it will do it inside the
warranty of an appliance designed to last decades.

The fix is straightforward and it is the right design anyway: **the display sleeps
and wakes on a button press.** Off by default, on for thirty seconds when someone
walks up and presses something. That removes burn-in as a concern, removes the
display from the power budget entirely, and matches how the thing is actually
used — nobody stands under a ventilation machine watching a temperature readout.

It does mean the display is dark the other 99.9 % of the time, which is worth
saying out loud, because "modern and stylish" usually means "lit".

### Readability where it actually is

- **LEDs** are readable across a dark room, from any angle, instantly, with no
  interpretation. That is not nostalgia, it is what an indicator is for.
- **A 0.96" OLED** has a 21.7 × 10.9 mm active area. Under a machine, that is a
  get-close-and-read-it interface. Excellent contrast, fine in the dark, but small.
- **A 1.14" TFT** is barely larger at 25 × 14 mm, and adds colour that this
  application has no use for.

If readability at distance were the goal, the answer would be LEDs or a 1.3"
display, not a 0.96" one.

### Firmware, which is a cost too

LEDs are eight bits into a shift register. A display is fonts, layout, screen
states, a menu, and a set of decisions about what to show and when — and it is the
part of a project that never feels finished. On a project whose value is the
protocol work, the electrical design and the measured evidence, a menu system
competes for attention with the things that matter.

A display also unlocks something LEDs cannot do at any price: **on-device
diagnostics**. Firmware rules already require a status output — firmware version,
uptime, last reset reason, bus statistics, free heap. On a display that is a
screen. On LEDs it is a blink code, and blink codes are how you make a device
hostile.

### Enclosure

A display needs a window, a bezel, alignment tolerance and something to hold the
glass. In an FDM print that is the hardest feature on the part. LEDs need holes and
a diffuser, or a translucent front printed in the same material — forgiving, and it
prints in one piece.

### Parity with what is being removed

This is the argument that decides it.

The original panel could set the heating setpoint from an eight-step 10…27 °C bar
on the wall, plus fan speed, CO₂ and humidity modes, post-heating, and whatever is
in its service menu. Matching that with LEDs means rebuilding a 2001 keypad: twenty
LEDs and eight buttons, about $0.75 in parts and a much larger board and front
panel.

Matching it with a display and three buttons is a menu.

**With Home Assistant present, none of this is needed locally.** Without it — the
network down, the broker moved, the house sold to somebody who does not run Home
Assistant — a display is the difference between a configurable machine and a
machine with two buttons.

## Recommendation

**Fit the 0.96" OLED, three buttons, and let the display sleep.**

- $2.55 per finished device, of which $0.24 is on the assembled board. On a device
  that costs perhaps €25 to build, that is not where the money is.
- It is the only option that gives local parity with the panel being removed
  without rebuilding a twenty-five-year-old keypad.
- On-device diagnostics — Wi-Fi state, IP address, bus error counters — is worth
  its price on its own in a technical space where the alternative is carrying a
  laptop under a machine.
- Sleeping removes burn-in and removes the display from the power budget.
- The board takes either option, so this is reversible.

**Design the board so the LED bar is not excluded.** Three indicator LEDs — power,
bus activity, fault — cost six cents, are readable from the doorway, and answer
"is it alive" without waking anything. That is the part of option A worth keeping,
and it is complementary rather than an alternative.

### The argument against, which is real

The thing being replaced has LEDs that outlived its own electrolytics. An OLED
fitted to a device meant to last as long as the machine will be the first thing to
fail, and a sleeping display is dark exactly when someone glances at it to check
the machine is running. If the goal were maximum service life with minimum
attention, option A wins and it is not close.

That argument loses here only because Home Assistant is the primary interface and
the local one is a fallback and a diagnostic tool. If that assumption changes, so
does the recommendation.

## What would change the answer

1. **M2a comes back low.** If the rail turns out to supply far less than the
   inference above, option C goes first and the OLED's sleep behaviour stops being
   a nicety.
2. **The device turns out to be looked at often.** Then readability at distance
   matters and a 1.3" display or an LED bar wins.
3. **Local parity is declared unnecessary** — the household accepts that setpoints
   are a Home Assistant thing. Then option A does everything needed for $0.36 and
   the display is decoration.

## New measurement this created

**M2a — measure the original panel's supply current**, with an ammeter in series
with the + wire, before the panel comes off the wall. Machine idle, machine at fan
speed 8, and with the panel's own LEDs at their brightest.

It is a two-minute measurement and it is the most valuable one available right
now: because the panel's regulator is linear, its input current *is* the rail's
demonstrated capability. It converts the central open question of this project —
how much can the bus give — from an inference about a heatsink into a number,
without loading anything or risking anything.

Added to [`measurement-plan.md`](measurement-plan.md).
