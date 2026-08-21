# Layout notes

Chronological, measurement-based log of schematic and PCB work. Written at the end
of every `/kicad-layout` session. Its value is in the numbers and in the dead ends:
"tried X, it did not work because Y" saves the next attempt.

## Template

### YYYY-MM-DD — <subsystem>

- **Baseline.** DRC / ERC violation counts and types before the session.
- **Changes.** What was moved, routed or replaced.
- **Measured.** Values before and after: trace widths, loop areas, clearances, lengths.
- **Did not work.** What was tried and rejected, and why.
- **Result.** DRC / ERC delta against the baseline. Zero new violations, or the reason.
- **Open.** What is left for the next session.

---

### 2026-08-22 — Schematic rev A, first draft (single A3 sheet)

- **Baseline.** Empty sheet. ERC 0 / 0 (nothing to check).
- **Changes.** Whole schematic drawn in one session: 60 components, 35 power
  symbols, 134 wire segments, 52 junctions, 19 net-name labels, 4 no-connect
  flags. Regions left to right: field terminal and power stage (top-left), RS-485
  front end (left-middle), buttons, display connector and USB-C (centre), ESP32-C3
  module with local pull-ups, LEDs and the backlight switch (right). Every signal
  is a continuous wire from pin to pin; labels only name nets.
- **Measured.** Not applicable — no layout. Values that went in from datasheets:
  TPS54202 VFB 0.596 V (SLVSDJ8 §6.5) → 100 kΩ / 13.3 kΩ for 5.08 V, the same
  pair TI uses in its 5 V design example; EN absolute maximum 7 V → divider
  100 kΩ / 15 kΩ, start ≈ 9.3 V at the 1.21 V rising threshold, 2.9 V at 22 V in;
  display backlight Vf 2.8–3.2 V at 80 mA (HS20HS072RX §3.2) → 27 Ω from 5 V,
  67–81 mA; IOVCC 1.65–3.3 V and VCI 2.4–3.3 V → 3.3 V is inside both ranges.
- **Did not work.**
  - `sch_build_circuit` with explicit `wires` produces exactly the drawn wires, but
    it **wipes the title block** — set it again afterwards — and it **does not add
    junctions**. A pin or power-symbol pin that lands on the interior of a wire is
    *not connected* in KiCad without one; ERC showed 40 "pin not connected" errors
    until 52 junctions were written into the file. Wire ends on wire interiors are
    connected without a junction.
  - `sch_get_pin_positions` reports rotation 90 **mirrored relative to KiCad**:
    for SM712 it put the common pin on the left, KiCad put it on the right; for
    D_Schottky it put the cathode at the top, KiCad at the bottom. Rotation 0 and
    180 agree. Consequence: the USB VBUS diode was drawn backwards at rot 90 and had
    to be set to 270; the TVS common wire had to be moved. Symmetric parts (R, C,
    L, fuse, switch) are unaffected. Verified from the netlist, not assumed.
  - No tool mirrors a symbol. J1 (field terminal) and U4 (transceiver) were
    mirrored by adding `(mirror y)` to the symbol instance in the file with KiCad
    closed; pin nets verified from the exported netlist.
  - `sch_render_png` fails on this machine (no libcairo). `kicad-cli sch export pdf`
    is the working route for a visual check.
- **Result.** `kicad-cli sch erc --severity-all`: 0 errors, 1 warning — Q1's
  embedded AO3400A symbol differs from the installed library copy (the MCP writes
  its own copy). Fix in the GUI with *Update Symbols from Library*. Netlist: 47
  nets, the only single-node nets are the four intentional no-connects (U3 NC,
  J3 pin 7, J2 SBU1/SBU2).
- **Open.**
  - Cosmetic: a few value labels overlap near D2/R5/JP1 and near Q1/R13; the
    J1 value sits under the symbol. GUI work.
  - The power stage is provisional until M1/M2a (see decisions).
  - Display tail contact side (top/bottom) is not stated in the datasheet text
    read so far; the FH12 connector is bottom-contact. Check the mechanical
    drawing before ordering, or switch to a double-sided-contact part.
  - DNP parts (R5 termination, R6/R7 bias) are marked in the value field only;
    set the DNP attribute in the GUI so the BOM export honours it.
- **Caught in self-review, same session.** The first draft put the button ladder
  on IO5, which is ADC2_CH0 — unusable with Wi-Fi. Moved to IO4 (ADC1_CH4); the
  display shifted to IO0–IO3 and LED_PWR to IO5. The EN capacitor C13 now shares
  C11's ground point so that the RST capacitor C9 has room. Netlist re-checked
  pin by pin after the change.
