# Component candidates

Parts considered before the schematic exists. This is not a bill of materials —
it is the reasoning that will produce one, written down while the alternatives are
still visible.

**Everything here is conditional.** Two measurements decide the power stage, and
neither has been made yet: whether the 21 V panel rail is isolated from mains (M1)
and how much current it can supply (M2). See
[measurement-plan.md](measurement-plan.md). Where a choice depends on a measurement,
the dependency is written next to it rather than resolved by guessing.

## Selection rules used

1. **Cheapest part that meets the requirement**, not the cheapest part.
2. **In the assembler's own inventory.** A part that has to be bought on the open
   market adds handling cost and lead time to a small run. JLCPCB *Basic* parts have
   no feeder setup fee; *Extended* parts do, currently around $3 per distinct part
   per order.
3. **Stock checked live, and checked again at order time.** Prices and stock below
   were read from the JLCPCB catalogue on **2026-08-21** and will have moved. They
   are recorded to make the reasoning checkable, not as a quotation.
4. **A second source for anything that is not a commodity passive.**
5. Prices are single-unit catalogue prices in USD. At a build of 5–10 boards the
   feeder fees dominate, which is why the Basic/Extended column matters more than
   the price column.

## Where the cost actually is

At these quantities the radio module is roughly 60 % of the parts cost and
everything else is noise. That has a consequence worth stating early: optimising the
passives is not where the money is, and the two places worth real attention are the
module choice and the number of distinct Extended parts.

## Architecture being costed

The board takes the place of the panel on the same five wires. It is the only
controller on the panel side of the bus.

```
 the five wires that fed          this board, on the wall where the panel was
 the original panel
 ┌────────────────────┐
 │ 1  +   22 V meas. ─┼──▶ reverse-polarity block ──▶ buck 3.3 V ──┬──▶ ESP32-C3
 │ 2  −               ┼──▶ return                                  │     │ UART+RTS
 │ 3  A               ┼──▶ PPTC ─┬─▶ RS-485, 1/8 UL, slew limited ◀─┘     │
 │ 4  B               ┼──▶ PPTC ─┤                                        │
 │ 5  M   signal gnd ─┼──────────┴─ TVS to bus ground                     │
 └────────────────────┘                                    local controls ─┤
                                                           USB-C (bench) ──┘
```

Half duplex, one UART, driver enable from the UART's RTS pin so the direction
change is timed by the peripheral rather than by software.

That last point is a deliberate answer to a known field failure. The community guide
reports that RS-485 modules **without** automatic direction control cause the Vallox
to show `Väylävika` (bus fault), and the usual fix is to buy a module with a
one-shot timer on the direction pin. Those timers are set for one baud rate and are
approximate. The ESP32 UART has a hardware RS-485 half-duplex mode that asserts the
driver enable around the transmission exactly, and using it costs nothing.

## Variant A — bus-powered, non-isolated

The baseline. Valid **only if M1 confirms the 21 V rail is a protective extra-low
voltage supply isolated from mains.** In that case the board has exactly one
galvanic connection to the world — the bus itself — and an isolation barrier inside
it would protect nothing during normal operation.

| Ref | Function | Candidate | LCSC | Stock | $/pc | Class |
|---|---|---|---|---|---|---|
| U1 | MCU + radio | ESP32-C3-MINI-1-N4 | C2838502 | 16 416 | 2.790 | extended |
| U2 | 21 V → 3.3 V buck | TPS54202DDCR | C191884 | 180 809 | 0.226 | extended |
| U3 | RS-485 transceiver | THVD1400DR | C3235232 | 93 954 | 0.320 | extended |
| D1 | Reverse-polarity block | SS34 | C8678 | 3 557 042 | 0.030 | **basic** |
| D2 | Bus transient clamp | PSM712-LF-T7 (SM712) | C32677 | 151 478 | 0.298 | **basic** |
| F1, F2 | Bus overcurrent, 100 mA / 60 V | JK-mSMD010-60 | C1884489 | 74 360 | 0.035 | extended |
| L1 | Buck inductor, 10 µH | FNR6045S100MT | C168076 | 31 949 | 0.073 | extended |
| J2 | USB-C 2.0 receptacle | TYPE-C-31-M-12 | C165948 | 336 394 | 0.161 | extended |
| J1 | 5-pole field terminal | KF128 family, 3.81 or 5.08 mm | C474933 and family | — | ~0.30 | extended, through-hole |
| — | Bulk, decoupling, dividers, LEDs, buttons | 0402/0603 basic parts | — | — | ~0.40 total | **basic** |

Parts cost lands around **$4.65 per board**, with the module at $2.79 of it. Local
controls, if fitted, add roughly $0.50 more.

### Why each of those

**U1 — ESP32-C3-MINI-1-N4, not the ESP32-S3 the scaffolding assumed.**
The workload is one 9600 baud UART, a Wi-Fi client and an MQTT connection. There is
no second core to use, no PSRAM to fill and no camera or USB host to drive. The C3 is
cheaper, draws less, and has the same native USB Serial/JTAG the S3 has, so it needs
no USB-to-UART bridge either. Given that the gating risk on this project is how much
current the bus can give (R2), the lower-power part is also the lower-risk part.

The module rather than the bare chip: a pre-certified module carries the radio
approval. A reference design that tells a reader to build one and put it on their
wall should not hand them an uncertified radio.

Second source: ESP32-C3-WROOM-02-N4 (C2934560, 3 702 in stock, $3.11) — different
footprint, so it is a design-time alternative, not a drop-in.

**U2 — TPS54202DDCR.** 4.5–28 V in, 2 A, integrated switch, and the number that
matters: **45 µA quiescent**. On a rail whose current budget is the main unknown, a
regulator that idles at milliamps is a bad trade. The JLCPCB *Basic* wide-input buck
is the TPS5430 (C9864, $0.81), which draws 4.4 mA quiescent — roughly a hundred
times more, and about 90 mW burned at 21 V doing nothing. Paying $3 of feeder fee to
avoid that is the right way round.

The caveat is the 28 V input ceiling. 21 V nominal leaves 7 V of margin, which is
comfortable for a regulated rail and thin for an unregulated one. **If M1 shows the
rail is unregulated or exceeds about 24 V at no load, this part is replaced** by
MP2459GJ-Z-MS (C45367195, 14 102 in stock, $0.40), which takes 55 V and costs 730 µA
of quiescent instead of 45 µA.

**U3 — THVD1400DR.** Chosen against the part it replaces rather than against the
price list. The original panel's transceiver was read off the board in the
photographs: a **MAX487**, which is 1/4 unit load, 48 kΩ receiver input impedance,
slew-rate limited, specified to 250 kbps. Three requirements follow:

- **Bus loading no heavier than the original.** The machine's driver was specified
  against a 1/4-unit-load panel. THVD1400 is 1/8 unit load, so the machine sees an
  easier bus after the swap, never a harder one.
- **A slew-rate limited driver.** The original limits its slew rate at 9600 baud on
  a cable that runs through a house. Fitting a 10 Mbps part instead would raise
  emissions for no benefit whatsoever.
- **Integrated fail-safe** on an open, short or idle bus. This removes two bias
  resistors and, more usefully, removes a question: whether the machine biases the
  bus stops mattering for the receiver's idle state.

Datasheet figures from TI SLLSF78B: 3–5.5 V supply, bus pins −16 V to +16 V
absolute maximum, ±12 kV IEC contact ESD, 700–900 µA quiescent while receiving.

The cheaper option is SP3485EN-L/TR (C8963, 327 071 in stock, $0.176) and it is a
JLCPCB *Basic* part, so it saves the feeder fee as well as the 14 cents. It is the
fallback, not the default — and if it is ever substituted, it has to be checked
against all three requirements above and not just against the pinout.

Note what the transceiver choice does **not** do: its bus pins are rated to ±16 V,
and a miswired supply would put 22 V on a data line. The fuse and the clamp below
are what handle that. Picking a tougher transceiver is not a substitute for them.

**D1 — SS34.** Reverse polarity on the 21 V pair is the failure the Vallox manual
puts in bold. A series Schottky is the crude answer and, at this current, the right
one: at an expected 10 mA average draw the forward drop is around 0.25 V and the
loss is a few milliwatts. An ideal-diode P-channel MOSFET would drop less and cost
more, and there is nothing here to spend that saving on. Basic part, three and a half
million in stock, three cents.

**D2 and F1/F2 — the bus fault path.** This is the part of the design that answers
the manufacturer's warning properly. The SM712 is a TVS made specifically for RS-485:
asymmetric, clamping the bus lines to about +13.3 V and −8.5 V, which sits just
outside the RS-485 common-mode window and just inside the SP3485's absolute maximum
of −7.5 V to +12.5 V. On its own it would be destroyed by 21 V applied continuously
to a data line, so it is paired with a 100 mA / 60 V resettable fuse in each line:
the TVS survives the transient, the fuse trips and turns a wiring mistake into an
inconvenience instead of a dead board.

`PSM712-LF-T7` at $0.298 is the Basic version; functionally identical Extended parts
sell for $0.03. Which is cheaper depends entirely on batch size — below roughly 12
boards the Basic part wins because it avoids the feeder fee, above that the Extended
one does. Decide at order time, from the actual quantity.

**L1 — FNR6045S100MT.** 10 µH, shielded, 2.7 A saturation, 62 mΩ, in a 6 × 6 mm
package. Oversized for a 500 mA load on purpose: the margin is free at seven cents
and the low DCR helps efficiency at the light loads this board actually runs at.

**J1 — the field terminal.** Five poles, screw or pluggable, in the manufacturer's
order `+ − A B M`, because that is exactly what is on the original panel's board
and the installer is transferring wires one at a time from one terminal to the
other. Landing 0.5 mm² solid conductors rules out anything spring-loaded and fine.

**The silkscreen labels by function, not by colour.** The manual's colour code is
orange-and-white NOMAK; the cable actually installed on the target machine is red,
blue, green, yellow and white. Every published guide for this bus describes the
wiring by colour, and in this house the colours are wrong. Printing a colour on the
board would be printing a mistake.

It is also the one through-hole part. JLCPCB charges per through-hole joint, so five
poles is a real line item, and the alternative — hand-soldering one connector — is
the difference between a fully assembled board and a board that needs a soldering
iron. For a reference project that tells people to build one, paying for the joints
is the right answer.

**Bulk capacitance.** Sized from the burst, not from a rule of thumb. Calculated:

- ESP32-C3 transmit peak is about 350 mA at 3.3 V, so 1.16 W, drawn in bursts of a
  few milliseconds.
- Through an 85 % efficient buck from 21 V that is about 65 mA on the bus side.
- Holding that for 2 ms within a 0.6 V droop needs `C = I·t/ΔV` ≈ 220 µF.

A 220 µF / 35 V part covers it with margin. **This calculation is not a measurement**
— the real burst profile comes from M2, and if the rail turns out to be stiffer or
weaker than assumed, this number moves.

### Deliberately not fitted, but footprinted

- **120 Ω bus termination**, behind a solder bridge, unfitted by default. The
  replacement should present the bus the panel presented, so the question is what
  the original did as much as what the machine expects. Answered by M1 and by a
  closer look at the original board now that it is coming off the wall.
- **Fail-safe bias resistors.** Footprinted and unfitted. The chosen transceiver
  biases itself, and adding external bias to a bus the machine may already bias is
  a way to make things worse.
- **Common-mode choke** on A/B. Cheap, helps conducted emissions, and easy to short
  out with two zero-ohm links if it turns out to be unnecessary.

## The local controls question, and what it would cost

Removing the panel removes the only way to change fan speed without a network.
Whether the replacement gets buttons is written up in
[target.md](target.md); here is what each answer costs in parts.

| Option | Parts | Approx. cost |
|---|---|---|
| Network only | nothing | 0 |
| Two tactile buttons and an 8-LED bar graph | 2 × SMD tact switch, 8 × 0603 LED, 8 × resistor or a shift register | under 0.50 € |
| Rotary encoder and a 0.96" I²C OLED | encoder with switch, OLED module | about 1.50 € |

The middle option is the recommendation and it is close to free against a 4.50 €
board. The bar graph also has a second use: it mirrors what the original panel
showed, so a household that has read fan speed off eight LEDs for twenty years
keeps reading it off eight LEDs.

Parts are not selected here. The board outline and the enclosure decide the
switch and LED footprints, and both are downstream of measurements that have not
happened.

## The enclosure is now a wall panel

This changed with the scope. The device is no longer a box beside the machine; it
is the thing on the wall where the panel was, in a living space, replacing
something that was designed to be looked at.

The manufacturer's drawing gives the original as **90 × 110 × 23 mm**, surface
mounted on a wall or over a one-gang box. Matching that footprint or covering it is
worth doing: whatever was behind the old panel — paint edges, a wall box, screw
holes — is what the new one has to hide.

That is a mechanical decision and it belongs to `/kotelo`, but it belongs in the
component list too, because it is what makes the through-hole terminal, the button
footprints and the board outline a single problem rather than three.

## Variant B — isolated bus front end

Required if **M1 shows the panel rail is not isolated from mains**, and worth
considering anyway if the board is expected to be plugged into USB while it is wired
to the machine.

| Ref | Function | Candidate | LCSC | Stock | $/pc |
|---|---|---|---|---|---|
| U3 | Isolated RS-485 transceiver | CA-IS3082CWX | C41784141 | 196 | 0.937 |
| U4 | Isolated 5 V for the bus side | B0505S-1W | C7465127 | 45 180 | 0.633 |

Delta over variant A is roughly **+$1.60 per board**, plus board area, plus a second
regulator stage to make the 5 V the isolation module needs.

The stock figure on the isolated transceiver is the problem: 196 pieces is not a
supply, it is a coincidence. If this variant is chosen, the part is re-selected
against stock at that time rather than taken from this table.

Note also what this variant does *not* buy. If the board is powered from the bus,
isolating the transceiver only moves the barrier — the power still crosses it. A
genuinely isolated design means an isolated DC-DC carrying the whole board's power,
which is the +$1.60 above, or an external supply, which makes the board ordinary.
Variant B is a contingency, not an upgrade.

## Rejected, and why

| Considered | Rejected because |
|---|---|
| ESP32-S3-WROOM-1 | $1 dearer than the C3 for capability this device has no use for, and a higher idle draw on a rail whose budget is the main risk |
| Bare ESP32-C3 chip with a PCB antenna | Saves about $1.50 and costs radio certification. Wrong trade for a design other people are told to build |
| TPS5430 (the Basic wide-input buck) | 4.4 mA quiescent, about 90 mW at 21 V doing nothing, on the one rail that cannot spare it |
| MAX485 and its clones | 5 V part; the whole board is 3.3 V |
| SP3485EN-L/TR as the default transceiver | Cheaper and a Basic part, so it saves the feeder fee too. Kept as the fallback, not the default: the original panel uses a 1/4-unit-load slew-limited MAX487, and matching that is worth 14 cents |
| RS-485 module with an automatic direction timer | The reported cause of `Väylävika` reports is direction timing, and the ESP32 UART already does it in hardware and exactly. Buying a module to solve a problem the MCU does not have is backwards |
| Ideal-diode controller for reverse polarity | Correct at amperes. At ten milliamps a three-cent Schottky is the same answer for less money and fewer parts |
| Barrel jack and an external supply | Removes the reason this device is better than a development board taped inside the machine |

## Open, and blocking

1. **M1 and M2.** The rail's isolation and its current capability. Variant A or B,
   TPS54202 or MP2459, and the bulk capacitor all hang on these.
1b. **Local controls: yes or no, and which.** It changes the board outline, the
   enclosure and about half a euro of parts. The recommendation is speed up, speed
   down and an eight-LED bar; the reasoning is in [target.md](target.md) and in
   risk R10.
2. **The through-hole terminal.** Cost per joint at the intended batch size decides
   whether it is assembled or shipped loose.
3. **Basic versus Extended for the SM712.** Decided by batch size at order time.
4. **Enclosure before final layout**, per the workspace rule that mechanics comes
   before placement — the terminal block's position and the antenna keep-out are
   enclosure decisions, not layout decisions.

## Which fab, and an inconsistency worth naming

Every part above was checked against the **JLCPCB** catalogue, because the whole BOM
sits in that assembler's own inventory and none of it is safety-critical — which is
exactly the condition under which JLCPCB is the cheaper route for a small batch.

`project.yaml` currently says `assembly.fab: pcbway`, which is the workspace default
and does not match that analysis. It is left alone deliberately: the order tooling
generates a PCBWay package and nothing else today, so changing the field would
produce a package for the wrong fab rather than fix anything. The route is decided
at the fabrication stage, and if it is JLCPCB then the tooling gap gets closed first.

## Stock check before ordering

The stock figures above are a snapshot and will be stale by the time a board is
ordered. The pre-order gate in the workspace rules re-runs the whole BOM against
live stock, checks the manufacturer part number against the designator type, and
flags every Extended part — that gate is what decides whether the board is ordered,
not this document.
