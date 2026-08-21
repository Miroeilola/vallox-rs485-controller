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

### C — Colour IPS TFT, ST7789 over SPI

| Part | Size | Active area | LCSC | Stock | $/pc |
|---|---|---|---|---|---|
| N114-2413THBIG01-H13 | 1.14", 240×135 | 24.9 × 14.0 mm | C2890618 | 421 | 2.536 |
| **HS20HS072RX** | **2.0", 320×240** | **40.8 × 30.6 mm** | **C5329582** | **657** | **3.772** |
| HS24CG003-ARX | 2.4", 320×240 | 49.0 × 36.7 mm | C5329583 | 108 | 3.960 |
| HS280S030RX | 2.8", 320×240 | 57.6 × 43.2 mm | C5329584 | 773 | 5.256 |

Plus an FPC connector at about $0.18 and three switches at $0.02.

**The backlight needs a driver, and this is the one real complication.** From the
datasheet for the 2.0" part: **four white LEDs in parallel, 80 mA typical.** White
LED forward voltage is around 3.0–3.2 V, so a 3.3 V rail leaves no usable headroom,
and parallel LEDs fed through one resistor will current-hog. Resolved either with a
small boost driver (about $0.30 plus an inductor) or with a 5 V intermediate rail
and a PWM-dimmed constant-current sink (about $0.05, but it changes the power
chain). Costed properly in the recommendation below.

The 2.4" has 108 pieces in stock, which is a coincidence rather than a supply. The
2.0" and 2.8" are the two with real availability.

### D — E-paper

**Not available.** There is no e-paper in the fab's catalogue, so it would be an
open-market purchase with its own lead time. It is also the wrong technology here
for a reason that has nothing to do with price: e-paper is reflective and needs
ambient light, and this panel lives under a machine in a room that is dark unless
somebody turns the light on. Ruled out.

## Is 0.96" big enough? No — and the arithmetic says so plainly

This file first recommended a 0.96" OLED. Working out the readable text size
showed that to be wrong, so the recommendation changed. The reasoning is left in
place rather than rewritten, because the arithmetic is the useful part.

### How the numbers are derived

Legibility is an angular quantity, not a linear one. A character subtending about
**20 arcminutes** is comfortable to read; about 10 arcminutes is the edge of
legibility. Twenty arcminutes works out to a character height of roughly
**viewing distance ÷ 170**.

Realistic viewing distance here: someone walks up to the machine, presses a
button, and reads — so **arm's length, about 50 cm**. At 50 cm, a comfortable
character is 2.9 mm tall.

### What each display gives at arm's length

Characters per line and lines per screen, at a font size that is comfortable at
50 cm:

| Display | Active area | Pitch | Chars × lines at 50 cm | $ | $/100 mm² | Stock |
|---|---|---|---|---|---|---|
| 0.96" OLED 128×64 | 21.7 × 10.9 mm | 0.170 mm | **8 × 3** | 2.31 | 0.98 | 2 467 |
| 1.14" IPS 240×135 | 24.9 × 14.0 mm | 0.104 mm | 9 × 4 | 2.54 | 0.73 | 421 |
| 1.3" OLED 128×64 | 29.4 × 14.7 mm | 0.230 mm | 11 × 4 | 5.33 | 1.23 | 632 |
| 1.54" OLED 128×64 | 35.1 × 17.5 mm | 0.274 mm | 13 × 5 | 6.46 | 1.05 | 520 |
| **2.0" IPS 320×240** | **40.8 × 30.6 mm** | 0.128 mm | **16 × 9** | **3.77** | **0.30** | **657** |
| 2.4" TFT 320×240 | 49.0 × 36.7 mm | 0.153 mm | 19 × 10 | 3.96 | 0.22 | 108 |
| 2.8" TFT 320×240 | 57.6 × 43.2 mm | 0.180 mm | 22 × 12 | 5.26 | 0.21 | 773 |

**Eight characters by three lines is not a menu.** It is enough for `SPEED 3` and a
temperature, and nothing else. The settings this device has to expose — heating
setpoint, bypass, pre-heat, supply-fan stop, fan speed limits, service counter —
have names that do not fit in eight characters, and a menu that scrolls three
lines at a time through eight-character abbreviations is a worse interface than
the LED bar it replaced.

Single large values are a different story. A 32-pixel digit on the 0.96" is 5.4 mm
and comfortable at 92 cm, so a fan-speed readout works fine. It is the *menu* that
does not fit, and the menu is the reason a display was chosen over LEDs at all.

### The price-per-area ladder is the real finding

Mono OLED costs about **$1.00 per 100 mm²** and gets worse with size — the 1.3"
is 1.8× the area of the 0.96" for 2.3× the price. Colour TFT costs **$0.21–0.30
per 100 mm²**, three to five times better, and improves with size.

The consequence is blunt: **the 2.0" colour IPS is 5.3× the screen area of the
0.96" OLED for $1.46 more.** The 1.3" mono OLED is 1.8× the area for $3.02 more.
Paying more for the smaller screen is not a trade-off, it is a mistake.

### What changes with a TFT, in both directions

**Better:**

- **No burn-in.** This removes the sleep requirement, and with it the objection
  that a sleeping display is dark exactly when someone glances over. The backlight
  can idle dimmed and go to full brightness on a keypress, which is a strictly
  better interaction than off/on.
- Colour is not needed but it is not useless either: red for a fault, green for
  running, at no extra cost.
- IPS has a full viewing angle, which matters for a panel mounted under a machine
  and therefore looked at from below or from the side.

**Worse:**

- **The backlight needs a driver.** Confirmed from the LCSC datasheet for
  HS20HS072RX: **four white LEDs in parallel, 80 mA typical.** White LED forward
  voltage is about 3.0–3.2 V, so there is no usable headroom from a 3.3 V rail and
  parallel LEDs cannot be fed through a single resistor without current hogging.
  Two ways out, and this needs deciding with the supply topology rather than after
  it:
  1. A small boost LED driver from 3.3 V — about $0.30 plus an inductor.
  2. A **5 V intermediate rail** (22 V → 5 V buck → 3.3 V), with the backlight on a
     PWM-dimmed constant-current sink from the 5 V. Headroom 1.9 V, a transistor
     and two resistors, about $0.05 — but it changes the whole power chain, which
     is a decision that belongs with M2a.
- **Backlight power becomes the largest single load.** 80 mA at 5 V is 0.4 W
  against an estimated 1.1 W budget. Dimmed to 20 % for the idle state it is
  0.08 W. That is affordable, and it is another reason M2a matters.
- Temperature rating is −20…+70 °C against the OLED's −40…+70 °C. Indoors in a
  technical space, irrelevant.
- Backlight LEDs degrade, roughly 30 000–50 000 hours at full current. Run dimmed,
  that is decades. It is a slower failure than OLED burn-in by an order of
  magnitude.

**Firmware:** 320 × 240 in colour does not need a frame buffer for a status
display — text and rectangles drawn straight over SPI use almost no RAM. It only
becomes an ESP32-C3 memory question if a graphics library gets involved, which is
a reason to keep the interface plain.

## The comparison that matters

### Cost, in context

LEDs and buttons come to $0.36. A 2.0" colour TFT with its connector, switches and
backlight drive comes to about $4.15. The rest of the board is about $4.65, so the
display roughly doubles the parts cost — and on a finished device with a PCB,
assembly, an enclosure and shipping, it is somewhere near 15 %.

That is a real number and it deserves to be said without softening. It is also not
the number that decides, because the thing being bought is not a screen, it is the
ability to set the machine's temperature setpoints without a network.

The cost fact that matters structurally is that **the display barely touches the
PCBA order**: about $0.30 of connector and switches, plus whatever the backlight
drive turns out to be. Whoever builds one decides at assembly time whether to fit a
display, and the same board takes either.

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

For a mono OLED the fix is to sleep: off by default, on for thirty seconds when
someone presses something. It works, and it means the display is dark the other
99.9 % of the time — worth saying out loud, because "modern and stylish" usually
means "lit".

**A TFT does not have this problem at all.** Liquid crystal does not burn in, and
its backlight LEDs degrade over 30 000–50 000 hours at *full* current, which is an
order of magnitude slower and slower still when dimmed. So a TFT idles dimmed and
goes to full brightness on a keypress, which is a better interaction than off and
on, and it removes the objection entirely rather than working around it.

This is one of the reasons the recommendation ended up on a TFT rather than the
OLED this file started with.

### Readability where it actually is

Worked out in full above. The short version: **LEDs** are readable across a dark
room from any angle with no interpretation, which is what an indicator is for and
is not nostalgia. **A 0.96" OLED** is an 8 × 3 character interface at arm's length.
**A 2.0" TFT** is 16 × 9 and a large fan-speed digit on it is readable from about
two metres.

The three indicator LEDs stay whichever display is fitted, because "is it alive"
should be answerable from the doorway.

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

**A 2.0" 320 × 240 colour IPS, three buttons, and three indicator LEDs.**

`HS20HS072RX`, LCSC **C5329582**, $3.77, 657 in stock, active area
40.8 × 30.6 mm, module 51.8 × 36.2 mm, ST7789 over SPI, −20…+70 °C.

- **It is the right size.** Sixteen characters by nine lines at arm's length is a
  menu, a status page and a diagnostics screen. Eight by three is not.
- **It is the best value on the board.** 5.3× the screen area of the 0.96" OLED
  for $1.46 more, and cheaper per square millimetre than any mono OLED offered.
- **It does not burn in**, so it idles dimmed instead of dark and answers "is the
  machine running" from across the room without anyone touching it.
- **It barely touches the PCBA.** The FPC connector and the switches are about
  $0.30 of assembled parts; the display plugs in afterwards. The same board still
  takes a smaller display, or none.

Total for the local interface: about **$4.15** per finished device, against $4.65
for the rest of the board. That is a real fraction of the parts cost and a small
fraction of what a built device costs.

**Keep the three indicator LEDs** — power, bus activity, fault. Six cents,
readable from the doorway, and they answer the only question that matters when
something is wrong without waking or lighting anything.

### The argument against, which is real

The thing being replaced has LEDs that outlived its own electrolytic capacitors,
and they are still lit after about twenty-five years. Any display fitted here will
be the first thing on this board to fail. If the goal were maximum service life
with minimum attention, option A wins and it is not close.

That argument loses because Home Assistant is the primary interface and the local
one is a fallback and a diagnostic tool — but it is why the three indicator LEDs
stay regardless of what display is fitted, and why the board has to work with the
display unplugged.

### Open before this is final

**The backlight drive.** Four parallel white LEDs at 80 mA cannot run from 3.3 V.
Either a boost driver at about $0.30, or a 5 V intermediate rail with a
constant-current sink at about $0.05 — and the second one changes the power chain,
so it is decided together with M2a and not before.

## What would change the answer

1. **M2a comes back low.** If the rail supplies far less than the inference above,
   the backlight is the first thing that has to go, and the answer drops back to a
   mono OLED that sleeps — or to LEDs.
2. **Local parity is declared unnecessary** — the household accepts that setpoints
   live in Home Assistant. Then option A does everything needed for $0.36 and any
   display is decoration.
3. **The viewing distance turns out to be much longer than arm's length.** If the
   machine is high enough that the panel is read from two metres, no display in
   this list is right and the answer is a large LED bar.

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
