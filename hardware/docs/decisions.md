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

### 2026-08-21 — Bus client, not a panel replacement

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

### 2026-08-21 — Proposed: ESP32-C3 instead of the scaffolded ESP32-S3

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
