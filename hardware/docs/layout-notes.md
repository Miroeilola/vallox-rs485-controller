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

### 2026-08-22 — Rev A placement, first pass (no routing)

- **Baseline.** Empty board file (59 bytes, 0 footprints). ERC unchanged from the
  schematic session: 0 errors, 1 cosmetic warning. DRC: nothing to check.
- **Changes.** The board was populated from the schematic netlist with KiCad's
  bundled `pcbnew` Python (KiCad closed), not with the GUI *Update PCB from
  Schematic*: the MCP server had no `pcb_write` tools this session (KiCad was not
  open when it started, so IPC never came up) and the one MCP tool that does a
  file-based transfer is the one the workspace rules forbid. Every footprint
  carries its schematic symbol UUID as `path`, so a later F8 in the GUI should
  reconcile to *no changes* beyond field text — that check is still to be run.
  Outline 104 × 66 mm, 2 mm corner radius, four Ø3.2 mm NPTH at (3.5, 3.5),
  (3.5, 62.5), (100.5, 62.5) and (70, 3.5). All 60 schematic parts placed on the
  top side (single-sided assembly), 4 mounting holes added (H1–H4, no symbol).
  Floor plan, left to right: display glass reserve 51.8 × 36.2 mm at
  (8, 8)–(59.8, 44.2) with the four buttons centred under it (x = 14.5/27.5/40.5/
  53.5, y = 52, 13 mm pitch, symmetric about the glass centre x = 33.9); the three
  LEDs in a column right of the glass (x = 63.8, y = 10/14/18, PWR–BUS–FAULT);
  J3 FPC connector at (67, 26.1) with its FPC entry facing the glass (lip at
  x = 62.6, 2.8 mm from the glass edge); ESP32 module at (92, 7.1), antenna over
  the top edge; USB-C flush with the right edge at y = 26; the 5-pole terminal at
  the right edge with the wire-entry face flush with the edge (pins at x = 99.4,
  y = 35.2…55.5, order from the top M B A − +); RS-485 front end between the
  terminal and the module (fuses 3.5 mm from pins A/B, TVS next to pin M = GND);
  buck in the bottom-right corner (input cap C2 pads 2.5 mm from U2 VIN/GND,
  inductor pad 5.6 mm from the SW pin, bulk C1 beside them, D1 8 mm from J1 +).
  Silkscreen: button legends, LED names, terminal pin names, board name, rev,
  mironet.fi and the repo URL. Reference designators were auto-placed by a script
  (0.8 mm, avoids pads, other courtyards, texts and the edge; rotated where a
  column is dense) — 57/57 resolved, 47 without touching any courtyard.
- **Measured.** Antenna placement follows the Espressif hardware design guide
  (*Positioning a module on a base board*): the 6 mm antenna area hangs over the
  board edge, feed point at the edge, the footprint's keep-out polygon lies
  entirely off-board; the guide's 15 mm metal clearance is an enclosure input and
  is written on Cmts.User next to the module. Nearest metal on the board: the
  USB-C shell at ≥ 20 mm, the top-right mounting hole at (70, 3.5) is 13 mm from
  the module body — use a plastic screw there if in doubt. Module thermal vias
  changed on the board instance from Ø0.2 mm drill (library) to Ø0.3 mm drill /
  Ø0.6 mm pad because the project rules set min hole 0.3 mm; hole-to-hole
  0.48 mm, pad-to-pad 0.18 mm within the same GND paddle.
- **Did not work.**
  - `pcbnew.BOARD.GetDrawings()` is not iterable in KiCad 10.0.2's Python when the
    board has content; the builder therefore always starts from the empty file.
  - `PCB_TEXT.GetBoundingBox()` does not follow `SetTextAngleDegrees(90)` for
    footprint reference fields in this binding — the reference placer computes
    rotated boxes itself.
  - The MCP placement gate FAILs on heuristics that do not hold here: it treats
    U1's off-board antenna keep-out and J1/J2's edge-flush courtyards as "outside
    board" and "overlap", measures decoupling distance to the module *centre*
    (C10 is 3.0 mm from pin 1, 12.7 mm from the centre) and flags J3/JP1 for not
    being at an edge. `kicad-cli pcb drc` is the authority, as the rules say.
- **Result.** `kicad-cli pcb drc --severity-all --schematic-parity`: 0 errors,
  7 warnings — 6 × `silk_edge_clearance` on U1/J1/J2 whose library silkscreen
  crosses the edge by design, 1 × `lib_footprint_mismatch` on U1 (the thermal-via
  change above); schematic parity: 0 mismatches, 4 extra footprints (H1–H4,
  intentional); 138 unconnected items (not routed). The parity run caught two
  things the first build got wrong: footprints written without their library
  nickname (`CP_Elec_8x10.5` instead of `Capacitor_SMD:CP_Elec_8x10.5`) — every
  part was reported as mismatched until the FPID was set — and TP1/TP2/JP1,
  whose library footprints are *excluded from BOM* while the schematic symbols
  were not; the symbols were set `in_bom no` (the right answer for test points and
  a solder jumper) and ERC is unchanged at 0 errors, 1 cosmetic warning. Render and top-view SVG read
  by eye: floor plan as intended. **CI will be red on this branch** until the
  board is routed — the workflow's DRC step uses `--exit-code-violations`, which
  returns 5 for unconnected items as well (verified locally).
- **Open.**
  - Display tail: exit edge and length assumed (short edge, toward J3); contact
    side still open. If the tail exits elsewhere, only J3 moves.
  - Run F8 in the GUI once (Update PCB from Schematic, *delete footprints with no
    symbols* OFF) and confirm it reports no footprint add/remove.
  - Routing; then GND pour both sides with stitching, and the 15 mm antenna
    clearance into the enclosure brief.
  - Module overhang means the panel needs a gap or cutout beside U1 — say so in
    the PCBWay remarks.
  - Logo (`mironet-mark-mono.svg`) not yet on the silkscreen; GUI image import.
