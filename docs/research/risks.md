# Risks

What can stop this project, how likely it is, and what the way around it is. Ordered
by how much damage the risk does if it lands, not by probability.

## R1 — The panel bus is not isolated from mains

**If true:** the project stops in its present form. A device that a user wires
themselves cannot sit on a mains-referenced bus and also expose a network interface.

**Evidence either way:** four generations of Vallox wiring diagrams (Digit SE
12/99, Digit2 SE 2008, 121 SE 2011, ValloPlus 350 SE 2015), read 2026-08-22 as the
vector drawings they are, draw the panel supply as a separate 230 V / 16 VAC
secondary and name it a protective-voltage winding; the 1999 guide for the LED-panel
generation uses a physically separate control transformer. Every manual describes
panel wiring as a user-level job. No community report of an earth-referenced
adapter being damaged was found. What no document can show is a GND–PE bond on the
mainboard (PELV), which is what the measurement is now for.

**Way around:** measurement M1, before anything is connected. If it turns out to be
mains-referenced, the honest outcome is to publish that finding — it is more useful
to the community than a device would have been — and to redesign around a fully
isolated front end with an external supply.

**Status:** open as a check, no longer as a coin toss. Measurement M1 (voltage
and, unplugged, resistance to PE) stays first.

## R2 — The 21 V rail cannot power a Wi-Fi radio

**If true:** the board cannot be bus-powered, which removes most of what makes it
better than the existing breadboard solutions.

**Why it is plausible:** the rail was specified to run a panel with an LCD and a
keypad, plus up to five CO₂ sensors and two humidity sensors in the worst case. No
document states a current limit for the rail itself, but (2026-08-22) the control
transformer is a 14 VA part and its secondary is fused T800 mA, shared with the
mainboard, relays and damper motor — so the whole low-voltage side has well under
0.65 A at 21 V to share. An ESP32 transmitting draws a few hundred milliamps at
3.3 V in bursts, which is tens of milliamps at the rail; others have run exactly
that from this pair alongside the factory panel for years.

**Ways around, in order of preference:**

1. Size a bulk reservoir from the measured burst profile so the average draw is what
   the rail sees, not the peak. This is the normal answer and it is cheap.
2. Reduce the radio duty cycle: longer Wi-Fi listen intervals, no mDNS chatter.
3. Drop to a lower-power radio. This is a real option and it changes the MCU choice.
4. Accept an external supply. This is the outcome that makes the board ordinary, so
   it is last.

**Status:** open, gating. Measurement M2.

## R3 — Writing the wrong register damages the machine

**If true:** an expensive appliance in a house that needs ventilation.

**Way around, by construction rather than by care:**

- The firmware carries an explicit allow-list of writable registers. It starts with
  one entry. A register joins the list only when a measurement report exists for it.
- Every write is preceded by a read of the same register, and followed by a read
  back. A write that does not read back is an error, not a retry.
- Values are range-checked against the encoding, not just passed through. A fan
  speed that is not one of the eight thermometer codes never reaches the bus.
- Bit registers are read-modify-write against a fresh read, never against a cached
  value.

**Status:** contained by design. Confirmed at measurement M6.

## R4 — Reversed wiring destroys the board

**If true:** a user builds the device, wires it the way the cable happens to fall,
and it dies — which is exactly what the manufacturer warns happens to the factory
panel.

**Way around:** this is a design requirement, not a risk to be accepted. The 21 V
input survives reversed polarity, and the A/B pair survives being swapped without
damage — swapped data just means it does not communicate. Both are tested on the
bench before the board is ever connected to a machine, and the test goes into
`docs/measurements/`.

The board should also make the mistake hard: terminals labelled on the silkscreen in
the manufacturer's own order and with the manufacturer's own wire colours, so the
label matches the cable in front of the installer rather than a datasheet.

**Status:** design requirement, not open.

## R5 — The register map is model-specific

**If true:** the project works on one machine and produces wrong readings or wrong
behaviour on another, which is worse than not supporting it.

**Evidence:** already known to be true in part. Vallox 130 D is documented by
Tom-Bom-badil as needing different register numbers, and pvainio handles two
different temperature register sets and calls one of them "the newer protocol".

**Way around:**

- Scope the claim honestly: the README says which machine it was verified on and
  which registers answered, and nothing more. This is the kind of limitation that
  makes a reference project credible rather than weak.
- Make the firmware discover rather than assume: poll the map at start-up, record
  which registers answer, and publish only those.
- Provide a way for a user with a different model to produce a capture and send it
  back, and add per-model findings to the repository as they arrive.

**Status:** open, managed by scope rather than solved.

## R6 — A second client on the bus is not tolerated — CONFIRMED TRUE

**Established.** Prior testing on the target machine: with two controllers on the
bus, they override each other. The manufacturer's "up to three panels" applies to
three Vallox panels, which the mainboard keeps in step by broadcasting state to
the whole panel group. A client that polls on its own schedule is not part of that.

**Consequence:** the product changed. It is now a panel replacement, not an
addition. The decision record is in
[`../../hardware/docs/decisions.md`](../../hardware/docs/decisions.md).

**Status:** closed, and it created R10 and R11 below.

## R10 — The device is now load bearing

**If it fails:** nothing controls the ventilation. The machine keeps running at
whatever it was last told, which is the difference between an inconvenience and a
hazard — but nobody can change it until the device is fixed.

This risk did not exist in the previous design. It arrived with R6 and it is the
price of the only architecture the bus allows.

**Way around:**

- Local controls on the device, so that a network outage is not a loss of control.
  Analysed in [user-interface.md](user-interface.md); the answer is a sleeping
  0.96" OLED, three buttons and three always-on indicator LEDs, which also covers
  the setpoints the old panel could reach — see R11.
- The machine's own protections — frost protection, over-temperature thermostats,
  the defrost cycle — live in the machine's firmware and this device cannot reach
  them. Nothing this device does or fails to do turns into a safety problem, and
  the README should say so plainly rather than leaving a reader to wonder.
- A watchdog and a rollback on failed OTA, so that a bad update does not leave a
  wall panel dead. Already in [../security.md](../security.md).
- Keep the original panel. It is being removed, not destroyed, and a working
  spare in a drawer is the cheapest possible recovery path.

**Status:** open. The local-interface half of it is answered in
[user-interface.md](user-interface.md); the rest — watchdog, OTA rollback, keeping
the old panel as a spare — is firmware and process work that has not started.

## R11 — Every setting the panel could reach has to be reachable here

**If missed:** the household quietly loses the ability to set the heating
setpoint, the bypass temperature or the fan speed limits, and nobody finds out
until the season changes.

**Way around:** the register list in [protocol.md](protocol.md) is the checklist,
and each writable register joins the firmware's allow-list only with a measurement
report behind it. The gap between "what the panel could do" and "what the
replacement can do" is tracked explicitly rather than discovered later.

**Status:** open, and it conflicts productively with R3: R3 wants the write list
short, R11 wants it complete. The resolution is that it grows one measured
register at a time, never one guessed register at a time.

## R7 — Components are not available when the board is ordered

**If true:** a redesign at fabrication time, which is the most expensive place to
find out.

**Way around:** the parts are chosen against live stock at the fab before the
schematic is frozen, preference given to Basic/preferred parts in the assembler's own
inventory, and the stock check is repeated as a gate immediately before ordering
rather than trusted from the time of selection. Second sources are recorded for
every part that is not a commodity passive.

**Status:** managed. See [component-candidates.md](component-candidates.md).

## R8 — Warranty and liability

Modifying the machine, or connecting anything to its bus, voids the manufacturer's
warranty and is the user's decision. The README says so plainly, in its own section,
not in a footnote. The project makes no safety claim and no IP rating claim that has
not been tested.

The machine is a ventilation appliance in a home. A failure of this board must leave
the machine running as it was — the device is an observer and an occasional
commander, never something the machine depends on.

**Status:** documentation requirement.

## R9 — Testing happens in an occupied house

**If true:** somebody's ventilation is off for an afternoon, in the worst case in
winter.

**Way around:** short agreed test windows, the machine returned to a verified working
state at the end of every session, and no long unattended runs until the bench work
is finished.

**Status:** managed by process. See [measurement-plan.md](measurement-plan.md).
