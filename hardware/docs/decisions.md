# Design decisions

One entry per decision that would otherwise have to be re-derived later. Newest last.
A decision without a reason is a note, not a decision.

## Template

### YYYY-MM-DD — <what was decided>

**Context.** What problem forced a choice.

**Options.** What was considered, with the numbers that separated them.

**Decision.** What was chosen.

**Reasoning.** Why, with sources: datasheet page, measurement, calculation.

**Consequences.** What this constrains later, and what would make us revisit it.

---

### 2026-08-21 — Bus client, not a panel replacement  — SUPERSEDED same day

**Superseded by "Replace the panel" below.** Kept because the reasoning that led
here was sound and only the premise was wrong: the bus does not tolerate a second
controller. Deleting it would hide that the trade-off was considered.


**Context.** The Vallox DIGIT bus allows up to three control panels. The project
could either replace the factory panel or join the bus alongside it, and the two
lead to different products.

**Options.** Replace the panel: needs a display, a keypad, an enclosure that fits a
wall box, and it takes away the local control the household already uses. Join the
bus: no user interface at all, but the machine keeps working exactly as before if
the board fails.

**Decision.** Join the bus as a fourth client with no local user interface.

**Reasoning.** The failure mode decides it. A ventilation machine in an occupied
house must not depend on this board. As a listener with occasional writes, a failure
is invisible to the household; as the panel, a failure is a house with no
controllable ventilation. It is also the cheaper and smaller board, and the factory
panel stays as the fallback interface.

**Consequences.** The firmware must take a bus address that no factory panel uses,
and it must be passive by default. It also means the device is useless on its own —
the README has to say that it needs a Home Assistant or MQTT installation to be
worth anything.

---

### 2026-08-21 — Power the board from the panel bus

**Context.** The board needs 3.3 V. The panel connector offers approximately 21 VDC
on terminals 1 and 2, which is what the factory panel runs on.

**Options.** Bus power: one cable, one connector, nothing else to install. External
supply: a second cable and a wall wart, and the installation stops being "wire it to
the five terminals the panel used".

**Decision.** Bus powered. Conditional on measurement M2.

**Reasoning.** The whole point of the board over the existing development-board
solutions is that it lands on the panel terminals and nothing else. No source states
how much current the rail can give, so this decision is provisional and M2 is the
gate: if the rail cannot support the radio's average draw plus margin, this is
revisited, and the fallback order is written in risk R2.

**Consequences.** The regulator's quiescent current becomes a selection criterion,
the bulk capacitor is sized from the transmit burst rather than from habit, and the
firmware's Wi-Fi duty cycle becomes an electrical parameter rather than a
convenience setting.

**Update, same day.** Photographs of the original panel show it running from the
bus through a TO-220 **linear** regulator on a folded heatsink. A linear regulator
that needs a heatsink from 22 V is burning on the order of a watt, so the rail was
built to source at least the panel's own draw — sixteen LEDs, a microcontroller
and a transceiver — through that drop. That is a prior, not a number, and M2 still
has to produce the number. It points the right way, and it hands the project a
comparison worth publishing: a switching regulator should draw less from the bus
than the panel it replaces, while doing more.

---

### 2026-08-21 — Survive reversed wiring by design

**Context.** The Vallox Digit2 SE manual prints, in bold, next to the panel terminal
table: *"(+) johdon virheellinen kytkentä tuhoaa ohjainpaneelin!"* — miswiring the
positive conductor destroys the control panel. That is the manufacturer describing
its own product's behaviour.

**Options.** Accept the same behaviour and warn in the documentation. Or design so
that a wiring mistake costs nothing.

**Decision.** The board survives reversed supply polarity and 21 V applied to either
data line, and this is verified on the bench before the board is connected to a
machine.

**Reasoning.** A replacement that fails the same way as the part it replaces has not
improved anything, and this is the single most likely mistake an installer will
make — the failure the manufacturer had to print a warning about. It costs a
three-cent Schottky in series with the supply and about seven cents of fuse and TVS
on the data lines. That is not a trade-off, it is a rounding error.

**Consequences.** Series elements in both data lines, which slightly reduces the
differential swing, and a forward drop on the supply, which slightly reduces the
input headroom. Both are accounted for in the regulator's input range. The bench
test for this becomes a published measurement.

---

### 2026-08-21 — Hardware-timed driver enable, not an auto-direction module

**Context.** The community guide for connecting a Vallox to Home Assistant reports
that RS-485 adapters without automatic direction control make the machine display
`Väylävika`, a bus fault, and recommends buying a module with a direction timer.

**Options.** A transceiver module with a one-shot timer on the driver-enable pin, a
software-toggled GPIO, or the ESP32 UART's own RS-485 half-duplex mode, which drives
the enable from the RTS pin in hardware.

**Decision.** The UART peripheral's RS-485 half-duplex mode, driving the transceiver's
DE/RE pins from RTS.

**Reasoning.** The reported fault is a timing fault: the driver is released too late
and steps on the next talker, or too early and truncates the last stop bit. A
one-shot timer is tuned for one baud rate and is approximate. A software GPIO is at
the mercy of task scheduling. The peripheral does it exactly, for free, and the
timing is deterministic.

**Consequences.** DE and RE are tied together and driven from one pin, which fixes
the pin budget. The real gap between frames is measured in M4 before any transmit
code runs, so this decision gets checked against evidence rather than assumed to work.

---

### 2026-08-21 — Proposed: ESP32-C3 instead of the scaffolded ESP32-S3  — MODULE VARIANT REVISED 2026-08-22

**Context.** The project was scaffolded with the workspace default, ESP32-S3. The
workload is one 9600 baud UART, a Wi-Fi client and MQTT.

**Options.** ESP32-S3-WROOM-1-N4 at roughly $3.80, ESP32-C3-MINI-1-N4 at $2.79
(C2838502, 16 416 in stock on 2026-08-21), or a bare C3 chip with a PCB antenna at
roughly $1.

**Decision.** **Proposed**, not yet taken: ESP32-C3-MINI-1-N4.

**Reasoning.** Nothing in the workload uses a second core, PSRAM, or the S3's
peripherals. The C3 has the same native USB Serial/JTAG, so neither part needs a
USB-to-UART bridge. It is about a dollar cheaper and idles lower, and the gating risk
on this project is the current the bus rail can supply — so the lower-power part is
also the lower-risk one. A pre-certified module rather than a bare chip, because a
design other people are told to build should not hand them an uncertified radio.

This stays *proposed* because M2 could push it further. If the rail turns out to be
much weaker than assumed, the answer is not a different ESP32 but a different radio,
and deciding now would only have to be undone.

**Consequences.** `project.yaml` and the firmware target still say `esp32s3` and are
left alone until M2 closes this. Recorded here so the contradiction is visible rather
than forgotten.

**Update, same day.** The local interface grew a 320 × 240 display, four buttons
and three indicator LEDs, and the C3's pin budget went from comfortable to
twelve-of-thirteen — and only because the buttons were moved onto a resistor
ladder. It still fits, and the ladder is a reasonable design rather than a
workaround, but the margin is gone. Whichever way M2 pushes the power question,
the pin count is now a second thing weighing on the same decision. See the button
entry at the end of this file for the budget.

---

### 2026-08-21 — Isolation deferred to a measurement, not chosen up front

**Context.** The project's premise mentions a galvanically isolated RS-485 front
end. On a board whose only external connection is the bus it draws power from, an
isolation barrier inside the board protects nothing during normal operation — the
power crosses it too.

**Options.** Non-isolated, with protection on the bus lines. Or an isolated
transceiver plus an isolated DC-DC carrying the board's whole supply, about $1.60
more per board.

**Decision.** Deferred to measurement M1: whether the 21 V panel rail is isolated
from mains.

**Reasoning.** The Vallox internal wiring diagram labels the supply transformer as
an autotransformer *with a protective-voltage winding*, which reads as the panel rail
being SELV. That reading comes from a low-resolution scan and is the weakest claim in
the research that actually matters. If it holds, isolation buys nothing during
operation and the honest thing is a cheaper board plus a documented rule about not
connecting USB while wired to the machine. If it does not hold, isolation is not an
upgrade, it is mandatory — and so is a rethink of whether this device should exist in
a form a homeowner wires themselves.

**Consequences.** The schematic is not drawn until M1 is reported. Both variants are
costed in `docs/research/component-candidates.md` so the decision is a measurement
away, not a redesign away.

**Update, same day.** The original panel is not isolated either — its transceiver
and its logic sit on the bus's own ground, behind a linear regulator fed from the
supply pair. So the non-isolated variant is exactly what has hung on this wall for
two decades, which is an argument, though not a proof: the panel also has no USB
socket and nothing else to tie it to another ground. The earth measurement in M1
still decides, and it now decides mainly what the documentation is allowed to say
about connecting a laptop to the device while it is wired to the machine.

---

### 2026-08-21 — Replace the panel

**Supersedes "Bus client, not a panel replacement" above.**

**Context.** The earlier decision assumed this board could join the bus alongside
the factory panel, on the strength of the manufacturer's statement that up to
three panels are allowed. Prior testing on the target machine says otherwise:
**with two controllers on the bus, they override each other.** That is first-hand
evidence from the machine this project is built against, and it beats a line in a
manual.

The likely explanation is that three *Vallox* panels work because the mainboard
broadcasts state to the whole panel group and the panels mirror it rather than
compete. A third-party client that polls on its own schedule is not part of that
choreography. Either way the outcome is the same.

**Options.** Coexist with the panel — ruled out by the test. Replace the panel and
take its address. Or sit between the machine and the panel as a repeater, which
means two transceivers, a store-and-forward layer, and a device that can break the
panel when it fails.

**Decision.** The original panel comes off the wall. This device goes on in its
place, on the same five terminals, as the only controller on the panel side.

**Reasoning.** It is what the bus allows. It also removes a whole class of
problems that the coexistence design carried: no address negotiation, no listening
for a quiet window before transmitting, no risk of provoking a bus fault in normal
operation.

**Consequences, and they are not small.**

1. **The device is now load bearing.** The earlier design could fail invisibly
   and the household would not notice. This one cannot: if it stops, nothing
   controls the ventilation. The machine keeps running at its last setting, which
   is the good news, but nothing can change it.
2. **Everything the panel could set has to be settable here.** Heating setpoint,
   bypass, pre-heat, supply-fan stop, fan speed limits. The one-entry write
   allow-list stays as the mechanism, but its content has to grow — one register
   at a time, each with a measurement report behind it.
3. **Local controls become a real question**, because removing the panel removes
   the only way to change fan speed without a network. Written up in
   `docs/research/target.md`; the recommendation is speed up, speed down and a
   visible indication of speed, at minimum.
4. **The bus survey in the firmware changes purpose.** It no longer picks a free
   address. It verifies that nothing else is talking on the panel side, which is
   a precondition rather than a convenience.
5. **The enclosure replaces a panel, but not a living-room one.** The original is
   mounted under the machine, in a technical space, with no footprint to match and
   nothing to cover up. The manufacturer's 90 × 110 × 23 mm is a reference point,
   not a requirement, and the replacement is free to be whatever shape suits its
   contents.

---

### 2026-08-21 — Match the original's bus loading and slew rate

**Context.** The original panel's transceiver was read off the board in the
photographs: a **MAX487**. That is a 1/4-unit-load, slew-rate-limited part —
48 kΩ receiver input impedance, up to 128 nodes, specified to 250 kbps.

**Options.** Fit the cheapest 3.3 V transceiver in the assembler's basic
inventory, or match the characteristics the machine was designed against.

**Decision.** The replacement presents a bus load no heavier than 1/4 unit load
and uses a slew-rate-limited driver. Candidate: **THVD1400DR** — 1/8 unit load,
integrated open, short and idle bus fail-safe, 500 kbps slew-rate limited class,
3–5.5 V, bus pins rated −16 V to +16 V absolute maximum, ±12 kV IEC contact ESD
(TI datasheet SLLSF78B).

**Reasoning.** Three things follow from the original's choice, and none of them
are guesses:

- The machine's driver was specified against a 1/4-unit-load panel. Replacing it
  with something heavier changes what that driver sees. Replacing it with
  something lighter cannot.
- The original deliberately limits its slew rate at 9600 baud. Fitting a 10 Mbps
  part on a cable that runs through a house would raise emissions for no benefit.
- The integrated fail-safe removes two bias resistors **and** removes an open
  question from the design: whether the machine biases the bus stops mattering for
  the receiver's idle state.

It costs about 14 cents more than the cheapest basic part, and it is an Extended
part rather than a Basic one.

**Consequences.** The bus pins are still only rated to ±16 V, so 22 V landing on
a data line would exceed them. The resettable fuse and the SM712 clamp stay — the
transceiver choice does not replace the protection, it sits behind it.

If the transceiver is ever swapped for the cheaper Basic part on cost grounds,
that substitution has to be checked against all three requirements above, not just
against the pinout.

---

### 2026-08-21 — Local interface: a sleeping OLED, three buttons, three LEDs — REVISED same day

**Context.** With the panel removed, the machine has no local interface at all.
The panel it replaces sits under the machine in a technical space, so there is no
size to match and no aesthetic to preserve — the question is purely what the
interface is for.

**Options.** Costed against live JLCPCB stock in
`docs/research/user-interface.md`: an LED bar and buttons at about $0.36, a 0.96"
128×64 OLED at about $2.55 for the finished device, a 1.14" colour TFT at about
$3.05 with an unresolved backlight question, and e-paper, which is not in the
fab's catalogue and needs ambient light in a room that has none.

**Decision.** A 0.96" OLED that sleeps and wakes on a button press, three tactile
switches, and three always-on indicator LEDs for power, bus activity and fault.

**Reasoning.** Three things, in order of weight.

1. **Parity.** The panel being removed could set the heating setpoint from an
   eight-step bar graph on the wall. Matching that with LEDs means rebuilding a
   2001 keypad — twenty LEDs and eight buttons. A display and three buttons is a
   menu, and it is cheaper than the replica.
2. **Diagnostics.** Wi-Fi state, IP address, bus frame and error counters, the
   last reset reason. On a display that is a screen; on LEDs it is a blink code,
   and blink codes are how a device becomes hostile. The firmware rules already
   require this output to exist.
3. **It barely touches the board.** $0.30 of connector and switches is assembled;
   the display is a bare panel plugged in afterwards. The same board takes either
   choice, so this decision stays reversible far longer than most.

Sleeping is not a power saving, it is a lifetime decision. Passive-matrix OLED is
specified to half brightness in the region of 10 000 hours for lit pixels, which
is fourteen months of continuous operation, and a static fan-speed readout would
burn in well inside the life of the appliance. Off by default, thirty seconds on a
keypress, removes that entirely.

**The argument against, kept because it is good.** The panel being replaced has
LEDs that outlived its own electrolytic capacitors, and they are still lit in the
photographs after about twenty-five years. An OLED will be the first thing on this
board to fail, and a sleeping display is dark exactly when someone glances over to
check the machine is running. That argument loses only because Home Assistant is
the primary interface here and the local one is a fallback — and if that
assumption ever changes, so does this decision.

**Consequences.** The enclosure needs a display window, which is the hardest
feature to print well on an FDM part. The firmware acquires a menu, which is scope
that competes with the protocol work, so it is built last and kept small. The
three indicator LEDs stay regardless, because "is it alive" should not require
waking anything.

**Revised, same day — see the entry below.** The 0.96" size was chosen before
anyone worked out how much text fits on it. It fits eight characters by three
lines at arm's length, which is not a menu, and the decision above was made on
the strength of being able to present a menu. Superseded rather than deleted,
because the failure mode is instructive: the part was selected on price and
category before the requirement was quantified.

---

### 2026-08-21 — Local interface, revised: a 2.0" colour IPS

**Supersedes the entry above.** The reasoning there stands; only the size and the
technology change.

**Context.** The previous entry chose a 0.96" OLED because a display was needed to
present a menu of named setpoints. Nobody had worked out how much text a 0.96"
display actually shows.

**The number that changed it.** Legibility is angular: a comfortable character
subtends about 20 arcminutes, which is a character height of roughly viewing
distance ÷ 170. At arm's length — 50 cm, because someone walks up and presses a
button — that is 2.9 mm.

| Display | Chars × lines at 50 cm | $ | $ per 100 mm² |
|---|---|---|---|
| 0.96" OLED | **8 × 3** | 2.31 | 0.98 |
| 1.3" OLED | 11 × 4 | 5.33 | 1.23 |
| 1.54" OLED | 13 × 5 | 6.46 | 1.05 |
| **2.0" IPS** | **16 × 9** | **3.77** | **0.30** |
| 2.8" TFT | 22 × 12 | 5.26 | 0.21 |

Eight characters by three lines cannot show `SUPPLY FAN STOP` or a scrolling list
of settings. It shows a fan speed and a temperature, which is what the LED bar it
replaces already did — for a tenth of the price.

**Decision.** `HS20HS072RX`, 2.0" 320 × 240 colour IPS, ST7789 over SPI, LCSC
C5329582, $3.77, 657 in stock, active area 40.8 × 30.6 mm, −20…+70 °C. Three
tactile switches and three indicator LEDs as before.

**Reasoning.**

1. **Size.** 16 × 9 characters is a menu, a status page and a diagnostics screen.
   8 × 3 is a readout.
2. **Value.** Colour TFT costs $0.21–0.30 per 100 mm² and mono OLED costs about
   $1.00 and gets worse with size. The 2.0" is 5.3× the screen area of the 0.96"
   for $1.46 more. Paying more for the smaller screen is not a trade-off.
3. **No burn-in.** This removes the sleep requirement and with it the objection
   that a sleeping display is dark exactly when someone glances at it. The
   backlight idles dimmed and goes to full on a keypress.
4. **Viewing angle.** IPS, full angle, on a panel mounted under a machine and
   therefore read from below or from the side.

**Consequences, and one of them is open.**

- **The backlight needs a driver.** The datasheet gives four white LEDs in
  parallel at 80 mA typical. Around 3.0–3.2 V forward, there is no headroom from
  3.3 V and parallel LEDs cannot share one resistor without current hogging.
  Either a boost driver at about $0.30 plus an inductor, or a **5 V intermediate
  rail** with a PWM-dimmed constant-current sink at about $0.05. The second is
  cheaper and better but it changes the whole power chain, so **it is decided
  together with M2a, not before.**
- Backlight power becomes the largest single load: 80 mA at 5 V is 0.4 W against
  an estimated 1.1 W budget, or 0.08 W dimmed to 20 %. Another reason M2a matters.
- The local interface now costs about $4.15 of a roughly $8.80 parts total. That
  is a real fraction and it is stated plainly in `docs/research/user-interface.md`
  rather than softened.
- The board must work with the display unplugged, and the three indicator LEDs
  stay, so that a failed or unfitted display never means a dead device.

---

### 2026-08-21 — Buttons: four tactile switches on a resistor ladder  — PART REVISED 2026-08-22

**Context.** The bill of materials has carried `TS-1187A-B-A-B` since the LED
sketch, but it was carried rather than chosen — nobody had decided how many
buttons, what force, what plunger height, how they are actuated through a printed
enclosure, or how they connect to a microcontroller whose pins are nearly all
spoken for.

**How many, and which.** Four: `−`, `+`, `OK`, `←`.

On the home screen `−` and `+` change fan speed directly, with no menu. That is
the daily action and it is what the original panel had dedicated arrows for. `OK`
enters the settings menu and confirms, `←` backs out.

Three buttons would work if `←` became a long press on `OK`. It is rejected
because long-press is a convention that has to be taught, and this device will be
operated twice a year by somebody who has forgotten. The fourth button costs one
resistor and one hole.

**The pin budget is what decided the wiring, and it is tighter than expected.**
ESP32-C3-MINI-1 exposes GPIO0–10 and GPIO18–21: fifteen pins, of which GPIO18 and
GPIO19 are USB-Serial-JTAG and giving them up means giving up USB.

| Function | Pins |
|---|---|
| RS-485 TX, RX, DE | 3 |
| Display SPI: SCK, MOSI, CS, DC | 4 |
| Display reset | 0 — RC from the 3.3 V rail |
| Backlight PWM | 1 |
| Indicator LEDs | 3 |
| **Buttons** | **1** |
| **Total** | **12 of 13** |

Four buttons on four GPIOs would need sixteen pins and does not fit. So the
buttons go on a **resistor ladder into one ADC channel**: four resistors, one pin,
one spare left over.

ADC1 only — GPIO0 to GPIO4 — because ADC2 is unusable while Wi-Fi is running.
GPIO2 is a strapping pin, and GPIO0/GPIO1 are the 32 kHz crystal pins if one is
ever fitted, so the ladder lands on **GPIO3 or GPIO4**. Neither is a strapping
pin, so a button held at power-up cannot change the boot mode — which is what
makes "hold `←` while powering up" a safe way to trigger a factory reset.

**The part.** `TS-1187A-B-A-B`, LCSC **C318884**: JLCPCB *Basic*, $0.0197,
1 683 297 in stock, 5.1 × 5.1 mm SMD, 1.6 N, 100 000 cycles, gold contacts,
−30…+85 °C.

The reason this part rather than another is that **the whole TS-1187A family
shares one 5.1 × 5.1 mm four-pad footprint** and differs only in plunger height
(1.5 to 3.0 mm) and force (1.6 N or 2.6 N). The board can be laid out now and the
height and force chosen when the enclosure exists, with no board change. In stock
today: 1.5 mm/1.6 N (Basic, $0.020), 1.7 mm/2.6 N ($0.040), 2.5 mm/2.6 N ($0.040),
3.0 mm/2.6 N ($0.047).

**Actuation through the enclosure — a mechanical decision, named here so it is not
forgotten.** Three ways, in order of preference:

1. **A thin flexure printed into the front panel**, 0.6–0.8 mm of PETG over each
   switch. No holes, no loose parts, and nothing for dust to get through — which
   matters next to a machine whose whole job is moving air through filters. Needs
   a short plunger and a light switch, because the flexure adds its own force, so
   it points at exactly the 1.5 mm / 1.6 N part above.
2. Separate printed plungers in through-holes. Four loose parts, a tolerance stack
   and a dust path.
3. A 3 mm plunger poking through the front. Simple, but the front wall plus the
   air gap has to come to under 3 mm and the stem is exposed.

**No BOOT or RESET buttons.** The C3's native USB-Serial-JTAG resets the chip and
enters download mode over USB with no buttons at all. EN and GPIO9 get test pads
instead: no enclosure holes, no cost, and nothing on the front panel that is not
for the user.

**Capacitive touch was considered and rejected**, for two reasons and the second
is the real one. **ESP32-C3 has no capacitive touch peripheral** — ESP32, S2 and
S3 do, C3 does not — so it would need external touch ICs at about $0.06 a channel.
More importantly, this is the only control the household has for its ventilation.
A control with no tactile feedback, in a space with dust and the possibility of
condensation, that can false-trigger or stop responding for reasons the user
cannot see, is the wrong risk to take for the sake of a smooth front panel.

**A rotary encoder was considered and rejected for now.** Turning through a
nine-line list is a better interaction than pressing `−` sixteen times, and if the
menu turns out deep it should be reconsidered. Against it: `EC11L1525G01` is $1.27
against $0.08 for four switches — sixteen times the price of the entire button set
— it is through-hole and so carries per-joint assembly cost, it needs a shaft, a
knob and a large hole, and a detented encoder is the most wear-prone mechanical
part that could be put on a board meant to last as long as the machine.

**Consequences.**

- Four resistors and one ADC channel instead of four GPIOs. Debouncing is done on
  a converted value rather than an edge, and the firmware has to reject readings
  that fall between ladder steps rather than round them to the nearest button.
- The ladder cannot see two buttons at once. Nothing in this interface needs it.
- **This tightened the case for revisiting the microcontroller.** On an ESP32-S3
  there are pins to spare and the buttons would be four plain GPIOs. The C3 fits,
  but with one pin left, and that is worth weighing when M2a closes the power
  question and the C3-versus-S3 decision is finally taken.

---

### 2026-08-22 — Library coverage is a part selection criterion

**Context.** Parts had been selected on requirement, price, stock and JLCPCB
Basic/Extended class. Nothing in that list asks whether a KiCad footprint and a 3D
model already exist. Checking the shortlist against the installed libraries found
that the most important footprint on the board — the microcontroller module — had
neither.

**Decision.** Existing library coverage is a selection criterion alongside the
others: prefer the part that already has a footprint **and** a 3D model, whenever
one exists that meets the electrical and mechanical requirement.

**Reasoning.** A drawn footprint has to be validated against IPC-7351 and against
the manufacturer's land pattern, and a wrong one is found at assembly. A 3D model
is a precondition for the enclosure fit check that this workspace requires before
anything is printed — and a missing model is not visible as a problem until the
enclosure has already been designed around a guess.

**How it is applied.** Three cases, three different answers:

1. **Footprint and model both exist** — use the part.
2. **Footprint exists, model missing** — this is not a reason to change the part.
   Obtain the manufacturer's STEP, or build one, and put it in `mironet-hw-lib`.
3. **Neither exists** — the strongest reason to prefer a different part. If the
   part is kept anyway, the library work is recorded as work, not assumed.

The rule does not override an electrical or mechanical requirement. If library
coverage is only available on the wrong part, the right part wins and the library
is extended.

**Where to check.** On disk, not from memory:

- footprints: `<KiCad>/SharedSupport/footprints/<lib>.pretty/<name>.kicad_mod`
- models: `<KiCad>/SharedSupport/3dmodels/<lib>.3dshapes/<name>.step`

KiCad 10 ships `.step` only, not `.wrl`. Two MCP tools give false negatives here
and neither may be used as evidence that something is missing:
`lib_search_3d_models` returns nothing for everything because it searches the
footprint directory, and `lib_search_footprints` missed footprints that are
present on disk.

**Consequences.** Two parts changed as a direct result, recorded below. Two more —
the USB-C receptacle and the display — keep their part and gain a library task
instead.

---

### 2026-08-22 — ESP32-C3-WROOM-02 instead of ESP32-C3-MINI-1

**Context.** The proposal above chose `ESP32-C3-MINI-1-N4` and named
`ESP32-C3-WROOM-02-N4` as a design-time alternative on a different footprint.
Two things have changed since it was written, and both point the same way.

**Options.**

| | ESP32-C3-MINI-1-N4 | ESP32-C3-WROOM-02-N4 |
|---|---|---|
| LCSC | C2838502 | C2934560 |
| Price, 2026-08-21 as recorded | $2.79 | $3.11 |
| Price, 2026-08-22 live | **$3.84** | **$2.89** |
| Stock | 25 989 | 2 255 |
| KiCad footprint | none | `RF_Module:ESP32-C3-WROOM-02` |
| KiCad 3D model | none | present |
| Module size | 13.2 × 16.6 mm | 18.0 × 20.0 mm |

**Decision.** `ESP32-C3-WROOM-02-N4`, LCSC C2934560.

**Reasoning.** The original argument for the MINI was price, and the price has
inverted: the module is now $0.95 cheaper than the part that was chosen for being
cheaper. Independently of that, the MINI-1 is the only part in the design with
neither a footprint nor a 3D model, and it carries the largest and most
consequential land pattern on the board.

The size penalty costs nothing here. The panel is mounted under the machine in a
technical space, and it is already recorded that there is no enclosure footprint to
match and nothing to cover. 4.4 mm of extra width buys a verified land pattern.

Both are the same silicon: ESP32-C3, 4 MB flash, and the pin budget above is
unchanged — the WROOM-02 exposes the same GPIO0–10 and GPIO18–21.

**Consequences.**

- Stock is 2 255 against 25 989. That is enough for any realistic batch but it is
  not the same margin, and it is re-checked at the pre-order gate.
- The MINI-1 becomes the second source rather than the first, and it is a
  design-time alternative only: different footprint, so it is not a drop-in.
- The board outline grows by roughly 4 mm in each direction in the module area.
  Layout has not started, so nothing is lost.
- The antenna keep-out moves with the module and is re-derived from the WROOM-02
  datasheet, not carried over.

---

### 2026-08-22 — Buttons revised: 6 × 6 mm gullwing instead of TS-1187A

**Supersedes the part choice in "Buttons: four tactile switches on a resistor
ladder" above.** Everything else in that entry stands: four buttons, the `−` `+`
`OK` `←` assignment, the resistor ladder into one ADC1 channel, no BOOT or RESET
buttons, and the rejection of capacitive touch and of a rotary encoder.

**Context.** `TS-1187A-B-A-B` was chosen partly because the whole TS-1187A family
shares one footprint, so the board could be laid out before the enclosure existed.
That argument was right. What it missed is that the family has a KiCad footprint
but **no 3D model at all**, which defers a problem into the one activity that
cannot absorb it — fitting an enclosure to a real board.

**Options.** Restricted to 4-pad SMD tactile switches that have both a footprint
and a 3D model in the installed libraries. That is the 6 × 6 mm gullwing family,
which resolves to two land patterns that are **not interchangeable**:

| Footprint | Pad span | 3D model |
|---|---|---|
| `SW_SPST_PTS645Sx43SMTR92` | 7.96 × 4.5 mm | present, H4.3 mm |
| `SW_Push_1P1T_NO_E-Switch_TL3301NxxxxxG` | 9.10 × 4.5 mm | present, H4.3 mm |

Parts on the 6 × 6 gullwing pattern, read live 2026-08-22:

| LCSC | Part | Height | Force | Cycles | Stock | $/pc |
|---|---|---|---|---|---|---|
| C879454 | K2-6639SP-A4SC-04 | 4.3 mm | 2.5 N | 1 000 000 | 54 370 | 0.0456 |
| C2837531 | KH-6X6X5H-STM | 5.0 mm | — | 80 000 | 720 195 | 0.0188 |
| C7470150 | ZX-QC66-4.3TP | 4.3 mm | 2.6 N | 100 000 | 65 369 | 0.0288 |
| C221877 | PTS645SL50SMTR92LFS | 5.0 mm | **1.3 N** | — | 9 410 | 0.2831 |

For comparison, the part being replaced: TS-1187A-B-A-B, C318884, 5.1 × 5.1 mm,
1.5 mm high, 1.6 N, 100 000 cycles, JLCPCB *Basic*, $0.0197, 1 683 297 in stock,
footprint present, **no 3D model**.

**Decision.** Lay the board out on the **PTS645 land pattern**
(`SW_SPST_PTS645Sx43SMTR92`). Fitted part: **PTS645SM43SMTR92LFS**, LCSC
**C221880** — C&K's own part, 1.6 N, 4.3 mm, −20…+85 °C, 1 421 in stock, $0.532.

*Recorded earlier the same day as `K2-6639SP-A4SC-04` (C879454, $0.0456), and
changed when price was explicitly removed as a constraint. The clone is kept below
as the cost-reduction option, because it is the right answer if a batch ever makes
$0.49 a button matter.*

**Reasoning.**

1. **It is the part the KiCad footprint was drawn for.** `SW_SPST_PTS645Sx43SMTR92`
   is C&K's own land pattern, taken from C&K's datasheet. Fitting the genuine part
   removes the pad-span question entirely rather than answering it — see the note
   on that below, which this decision retires.
2. **1.6 N restores the force the flexure wanted.** The superseded entry chose
   1.6 N because a printed flexure adds its own force. The 6 × 6 clones are
   2.5–2.6 N; this part is 1.6 N. The compromise disappears instead of being
   deferred.
3. **The 3D model is exact, not approximate.** This part is 4.3 mm high and the
   KiCad model is the 4.3 mm variant. The cheaper KH-6X6X5H is 5.0 mm, which
   leaves the model 0.7 mm wrong in exactly the dimension that sets front-panel
   thickness and clearance. A model that is nearly right is worse than none,
   because it will not be questioned.
4. **A real second source.** C&K is stocked by Digi-Key, Mouser and Farnell. Every
   6 × 6 clone considered here is available from LCSC and nowhere else, which is a
   single point of failure on a part fitted four times per board.
5. **6 × 6 mm is a larger target than 5.1 × 5.1 mm**, pressed through a printed
   front panel. Larger is better here, not worse.
6. The same argument that favoured the TS-1187A family still holds and holds
   wider: one land pattern carries plunger heights from 4.3 mm to 9.0 mm — 4.7 mm
   of choice against the TS-1187A family's 1.5 mm.

**Other parts on the same pads**, if the enclosure or a cost target later argues
for one. The board does not change for any of them:

| LCSC | Part | Force | Height | Stock | $/pc |
|---|---|---|---|---|---|
| **C221880** | **PTS645SM43SMTR92LFS** | **1.6 N** | **4.3 mm** | 1 421 | **0.532** |
| C221871 | PTS645SK43SMTR92LFS | 2.6 N | 4.3 mm | 2 152 | 0.482 |
| C221877 | PTS645SL50SMTR92LFS | 1.3 N | 5.0 mm | 9 410 | 0.283 |
| C879454 | K2-6639SP-A4SC-04 | 2.5 N | 4.3 mm | 54 370 | 0.046 |
| C7470150 | ZX-QC66-4.3TP | 2.6 N | 4.3 mm | 65 369 | 0.029 |

The 1.3 N part is lighter still but 5.0 mm tall, so its 3D model is 0.7 mm off.
The clones are an order of magnitude cheaper and carry both the pad-span question
and single-sourcing.

**Consequences.**

- **Basic status is lost.** Every 6 × 6 option is Extended; TS-1187A was Basic.
  That is roughly $3 of feeder setup per order, not per board — about $0.30 a board
  across ten boards.
- Parts cost for the button set rises from $0.08 to **$2.13** per board. On a board
  whose parts come to about $4.85 that is a 44 % increase for four switches, and it
  is accepted deliberately: it buys the correct force, an exact model, a verified
  land pattern and a second source, on the only control the household has.
- Board area per button grows from 5.1 × 5.1 mm to 6 × 6 mm, four times over.
- **The pad-span verification that this entry originally listed as blocking is
  retired.** It existed because a clone's land pattern could not be inferred from a
  catalogue package string. Fitting C&K's own part makes KiCad's footprint and the
  part's datasheet the same document. If a clone is ever substituted for cost, the
  check comes back with it.
- The switch is 4.3 mm high against 1.5 mm. The board-to-front-panel distance
  changes accordingly and is an input to the enclosure, not a consequence of it.

---

### 2026-08-22 — USB-C stays inside the enclosure

**Context.** The USB-C receptacle `TYPE-C-31-M-12` (C165948) has an exact KiCad
footprint but no 3D model, and the search for a replacement that has both found
only one part in JLCPCB's catalogue: Amphenol `12401610E4#2A` (C5119948) at $1.334
against $0.161, 24 pins of USB 3.2 against 16 of USB 2.0, for a port this design
uses as a bench connection. Changing the part to gain a model is the wrong trade.

**Decision.** Keep `TYPE-C-31-M-12`, and place it **inside the enclosure** with no
opening in the wall. Reaching it means opening the box.

**Reasoning.** The port exists for bench work: first flash, console, and recovery.
None of those happen with the device installed under the machine. An opening in the
wall would add a dust path into an enclosure that lives beside a fan moving filtered
air, and dust ingress is the failure this enclosure is most exposed to.

That also settles what the missing 3D model is for. It is no longer needed to place
an opening to ±0.5 mm; it is needed for internal clearance — that the lid closes
over the receptacle and that nothing lands on top of it. That is a lower accuracy
requirement, and it does not change the decision to model the part, only its
urgency.

**Consequences.**

- **Field firmware updates are OTA-only.** This is already required by the
  workspace firmware rule, but it stops being a convenience and becomes the only
  path. Rollback on a failed update has to work, because the fallback is a
  screwdriver.
- **Recovery from a bad flash means opening the enclosure.** Combined with the
  earlier decision to fit no BOOT or RESET buttons and rely on the C3's native
  USB-Serial-JTAG, there is no way to recover a bricked device without taking it
  off the wall. Accepted: the alternative is a hole, a button, or both, on a front
  panel that should carry nothing that is not for the user.
- The enclosure needs no USB cutout, no gasket around one, and no strain relief for
  a cable that is never connected in service.
- Test pads for EN and GPIO9 remain worth having, and are now the only external
  electrical access besides the terminal block.

---

### 2026-08-22 — Schematic rev A drawn before M1/M2a, power stage marked provisional

**Context.** The plan was not to draw a schematic before the rail measurements.
Miro asked for something concrete in KiCad now. Roughly two thirds of the design
does not depend on those measurements at all, and the third that does can be
redrawn as a component swap rather than a topology change.

**Decision.** Draw the whole schematic now; mark the power stage provisional in
the title block and here; list exactly what M1/M2a can change.

**What M1 / M2a can still change.** The buck (TPS54202 → MP2459 if the rail is
unregulated or exceeds ~24 V unloaded); the bulk capacitor (220 µF is calculated,
not measured); the EN divider threshold; and whether the backlight is fed from a
5 V intermediate rail (drawn) or a dedicated boost. Nothing downstream of 3.3 V.

---

### 2026-08-22 — 5 V intermediate rail with a 3.3 V LDO, provisionally

**Context.** The backlight needs 2.8–3.2 V at 80 mA (HS20HS072RX datasheet §3.2)
and cannot run from 3.3 V with any control. The bench needs the board to run from
USB when no bus is connected. Both were open items deferred to M2a.

**Options.** (a) Buck to 3.3 V plus a small boost for the backlight, USB VBUS
diode-ORed into the buck input (4.7 V in is above the 4.5 V minimum). (b) Buck to
5 V, ME6211C33 LDO to 3.3 V, backlight from 5 V through a resistor and a
PWM-driven N-MOSFET, USB VBUS diode-ORed straight into the 5 V rail.

**Decision.** (b), provisionally. The schematic is drawn this way.

**Reasoning.** One regulator fewer to qualify (the LDO is a $0.05 SOT-23-5 part
with 60 µA quiescent, ME6211C33M5G-N, C82942), USB bench power becomes trivial,
and the backlight gets a real current path. The cost is efficiency on the logic
rail: the LDO drops 1.7 V, which at a 25 mA Wi-Fi-idle average is about 42 mW,
or ~2 mA from the 21 V bus. The boost option's own quiescent current is of the
same order. M2a decides whether that 2 mA matters; if the rail is starved, the
design flips to (a) and the change is contained in the power region.

**Consequences.** Second regulator on the board. LDO dissipation at the Wi-Fi
transmit peak (≈350 mA × 1.35 V ≈ 0.47 W) is momentary; at a sustained OTA
download (~150 mA) it is ~0.2 W in SOT-23-5 — measured in bring-up, not assumed.

---

### 2026-08-22 — Buck details: 22 µH, 100k/13.3k feedback, 100k/15k EN divider

- **VFB = 0.596 V** (TPS54202 datasheet SLVSDJ8, §6.5). 100 kΩ / 13.3 kΩ gives
  5.08 V and is the pair TI uses in its own 5 V design example (§8.2).
- **22 µH, not the 10 µH in the candidate list.** Inductor ripple
  ΔI = Vout·(1−D)/(L·f) at 21 V in, 5 V out, 500 kHz: 10 µH → 0.76 A p-p, larger
  than the load; 22 µH → 0.35 A. Same FNR6045S family, FNR6045S220MT, C168080,
  2.2 A saturation, 116 mΩ.
- **EN must not be tied to the input.** EN absolute maximum is 7 V (§5.1); the
  rail is 21 V. 100 kΩ / 15 kΩ: 2.9 V at 22 V in, 3.7 V at the 28 V operating
  maximum, and the converter starts at ≈ 9.3 V (1.21 V rising threshold, §6.5).
  A 9 V start keeps a sagging bus from brown-out cycling the radio.

---

### 2026-08-22 — ESP32-C3 GPIO map, decided by wire order

**Context.** On the WROOM-02 symbol fourteen I/Os sit on the left edge at 2.54 mm
pitch and two on the right. If functions are not grouped by physical adjacency
the schematic fills with crossings, and the same is true of the PCB.

| GPIO | Function | Note |
|---|---|---|
| IO0 | TFT_MOSI (SDA) | |
| IO1 | TFT_SCK (SCL) | |
| IO2 | TFT_DC (RS) | strapping; 10 kΩ pull-up — DC is don't-care between transfers, so the pull-up costs nothing |
| IO3 | TFT_CS | |
| IO4 | BTN_ADC | **ADC1_CH4**. ADC1 is GPIO0–4 only; GPIO5 is ADC2_CH0 and ADC2 is unusable while Wi-Fi runs. A first draft had the ladder on IO5 and was caught in review |
| IO5 | LED_PWR | |
| IO6 | RS485_RX (RO) | |
| IO7 | RS485_DE (DE and ~RE tied) | 10 kΩ pull-down keeps the driver off through reset |
| IO8 | RS485_TX (DI) | strapping; 10 kΩ pull-up, and UART TX idles high anyway |
| IO9 | BOOT test pad | strapping, internal weak pull-up (datasheet table 4-1) |
| IO10 | LED_FAULT | |
| IO18 / IO19 | USB D− / D+ | native USB-Serial-JTAG |
| IO20 | LED_BUS | right side; flickers with the ROM boot log, which is fine for a bus LED |
| IO21 | TFT_BL_PWM | right side; input at reset → backlight off until firmware says otherwise |

**Strapping, from the module datasheet §4.1.** GPIO2 must read 1 at reset
("recommended to pull this pin up due to glitches"); GPIO8 must read 1 for the
joint download boot that the first USB flash relies on; GPIO9 has an internal
weak pull-up. Hence the pull-ups on IO2 and IO8 and the test pad on IO9.

**Button ladder.** 10 kΩ pull-up, switches to ground through 0 / 1.5 k / 3.3 k /
6.8 kΩ: 0, 0.43, 0.82, 1.34 V pressed, 3.3 V idle. 100 nF on the node. The
order of the four display lines (IO0–IO3) and the button line (IO4) matches the
pin order on the FPC connector, which is why the schematic has no crossings there;
the ADC constraint above is what fixed the button on IO4 rather than IO0.

---

### 2026-08-22 — Display tail is 12-pin: FH12-12S-0.5SH replaces the 30-pin connector

**Context.** The candidate list carried a 30-pin FPC connector. The display
datasheet (HS20HS072RX, §6) lists **12 pins**: GND, CS, RS, SCL, SDA, RST, NC,
IOVCC, VCC, LED A, LED K, GND.

**Decision.** Hirose FH12-12S-0.5SH (C88360, $0.44, 5 077 in stock) — footprint
and 3D model ship with KiCad. Value field on J3 is the display part number.

**Open.** FH12 is bottom-contact. The tail's contact side is read from the
display's mechanical drawing before the order; if it is top-contact, the swap is
to a double-sided part and a library addition.

**Reset.** RST has a 10 kΩ / 1 µF RC from 3.3 V, no GPIO spent.

---

### 2026-08-22 — Symbol mirroring and a tool convention that was wrong

J1 and U4 are mirrored (`mirror y`) so that field wiring enters from the left
and bus pins face the terminal. The MCP's pin-position tool reports rotation 90
mirrored relative to KiCad; the netlist, not the tool, is what was checked.
Details in `layout-notes.md`.

---

### 2026-08-22 — Board floor plan: UI left, I/O right, antenna over the top edge

**Context.** Placement had to start before the enclosure exists (Miro's order:
parts → layout → enclosure), so the board itself fixes what the enclosure must
respect: wire entry, display position, button row, antenna clearance.

**Options.** Antenna flush with the edge on a copper-free area, or over the edge
as Espressif prefers; terminal block on the bottom edge under the buttons or on
the right edge; display glass resting on the board or held only by the front
panel.

**Decision.** 104 × 66 mm, single-sided assembly. Display glass area reserved on
the top side (component-free, the glass may rest on the board), four buttons
centred under it, LEDs beside it, FPC connector on the glass's short edge. The
module sits top-right with its antenna 6 mm over the top edge. Terminal block and
USB-C on the right edge, wire entry and plug from the right. Buck in the
bottom-right corner, RS-485 front end between terminal and module.

**Reasoning.** The Espressif hardware design guide marks the over-the-edge
antenna as the strongly recommended position and asks for ≥ 15 mm to metal in
the housing; putting the module in a corner with the display on the far side
keeps the glass's metal frame and every connector ≥ 20 mm away. One edge for all
field wiring keeps the enclosure's cable path to one wall and leaves the front
free for the user interface only. Single-sided assembly is the cheaper PCBWay
order and the board has the area for it; the original panel was 90 × 110 mm, so
104 × 66 mm is no regression in size.

**Consequences.** The enclosure brief inherits: display window centred on
(33.9, 26.1) from the board's top-left corner, button centres at y = 52, LED
column at x = 63.8, cable entry on the right wall, ≥ 15 mm of non-metal around
the antenna above the top edge, and a panel gap beside U1 for assembly. If the
display tail turns out to exit another edge, J3 moves and nothing else does.
