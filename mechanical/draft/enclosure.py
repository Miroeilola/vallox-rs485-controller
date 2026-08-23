# SPDX-License-Identifier: CERN-OHL-S-2.0
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""First-pass enclosure draft for the Vallox RS-485 controller.

Generates mechanical/step/enclosure-base-draft.step and
mechanical/step/enclosure-cover-draft.step (+ STLs) from board facts read out of
hardware/vallox-rs485-controller.kicad_pcb and mechanical/step/pcb.step.

This is the CAD-scripted space claim described in .claude/rules/mekaniikka.md.
Fusion 360 is the master for the final enclosure; these -draft files are the
starting geometry, not the deliverable.

Every dimension below is either read from the board file / STEP models or
carries its source in a comment. Verified against the exported pcb.step by a
boolean interference check at the end of this script.

Coordinate system: KiCad board XY (x right, y down in the layout editor) is used
for all parameters; the helper ky() maps to the STEP frame (y up). z=0 is the
inner floor of the base, +z toward the user.
"""

from build123d import (
    Pos, Rectangle, RectangleRounded, Cylinder, Cone, Compound,
    extrude, import_step, export_step, export_stl, ExportSVG,
)
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
STEP_DIR = os.path.join(HERE, "..", "step")
STL_DIR = os.path.join(HERE, "..", "stl")
PCB_STEP = os.path.join(STEP_DIR, "pcb.step")

# ---------------------------------------------------------------- board facts
# Source: pcbnew read of hardware/vallox-rs485-controller.kicad_pcb (2026-08-23)
BOARD_W, BOARD_H = 104.0, 66.0          # Edge.Cuts bbox
BOARD_T = 1.51                          # pcb.step solid Z extent (stackup); design setting says 1.6
HOLES = [(3.5, 3.5), (66.0, 3.5), (3.5, 62.5), (100.5, 62.5)]  # M3, drill 3.2
SW_XY = [(14.5, 52.0), (27.5, 52.0), (40.5, 52.0), (53.5, 52.0)]  # PTS645 centres
LED_XY = [(63.8, 7.8), (63.8, 10.8), (63.8, 13.8)]              # D4 D5 D6, 0603
# Display active area, world coords derived from the HS20HS072RX model in pcb.step:
# local VA 30.6 x 40.8 centred (0, +2.86), DS1 at (33.9, 26.1) rot 90.
VA_CX, VA_CY = 31.04, 26.1
WIN_W, WIN_H = 46.6, 33.6               # manufacturer-recommended window (HSD drawing)
# Component heights above board top, from the STEP models (KiCad packages3D +
# mironet-hw-lib display model):
H_J1, J1_PIN_BELOW = 13.8, 3.5          # MKDS 5x5.08 terminal block
H_C1 = 10.5                             # CP_Elec_8x10.5
H_SW = 4.3                              # PTS645 actuator top
H_TAB = 2.15                            # display frame-tab top (max of display stack)
H_GLASS = 2.03                          # display glass top
H_MODULE = 3.2                          # ESP32-C3-WROOM-02
# No STEP model existed for J2 (USB-C, lib issue #1) and F1/F2 (Fuse 1812).
# Assumed heights: USB-C 3.31 (HRO TYPE-C-31-M-12 drawing), fuse 0.85 (1812 typ).
# Both are far below the 6.0 mm main ceiling -> not load bearing for this draft.

# Antenna overhang: module local Y -6.9 from anchor (92, 7.1) -> spans y -6.0 .. 0
# beyond the top board edge, x 83..101. Keep conductive parts >= 15 mm away
# (Espressif HDG + project layout notes).

# ------------------------------------------------------------- design choices
WALL = 2.0                              # PETG wall
FLOOR = 2.5
PLATE = 2.5                             # cover face plate
STANDOFF = 4.0                          # board bottom to floor: J1 pins 3.5 + 0.5
BT = STANDOFF + BOARD_T                 # board top height above floor = 5.51
MAIN_CEIL = BT + 6.0                    # cover inner face: SW 4.3 + membrane gap
BLISTER_CEIL = BT + H_J1 + 1.1          # 20.41: J1 + 1.1 clearance
LIP_BOT = BT + H_TAB + 0.4              # window dust lip, 0.4 over frame tabs
NUB_GAP = 0.3                           # membrane nub to actuator at rest
MEMBRANE_T = 0.6
INSERT_D, INSERT_DEPTH = 4.0, 8.0       # M3 heat-set, typical Ø4.0 for M3
BOSS_INSERT_DEPTH = 6.0

# Cavity margins around the board
CAV_X0, CAV_X1 = -0.6, 118.0            # right side: 14 mm wiring bay for J1
CAV_Y0, CAV_Y1 = -8.0, 74.0             # top: antenna 6.0 + 2.0 air; bottom: posts
OUT_X0, OUT_X1 = CAV_X0 - WALL, CAV_X1 + WALL
OUT_Y0, OUT_Y1 = CAV_Y0 - WALL, CAV_Y1 + WALL
RIM_Z = MAIN_CEIL                       # base wall top = cover plate seat

# Cover screw posts (Ø8, merged into walls). Top-right corner is skipped on
# purpose: a corner post would put a steel screw ~13.6 mm from the antenna;
# the post moved to the right wall at y=18 (>= 22 mm from the antenna).
POSTS = [(2.4, -5.0), (2.4, 71.0), (115.0, 71.0), (115.5, 18.0)]

# Blister (raised cover section) over the tall parts, KiCad coords.
# R1 covers C1 (73.8..86.2, 54.0..63.0), R2 covers J1 (93.7..104.8, 32.1..58.6).
BLIS = [  # (x0, x1, y0, y1) outer; cavity inset by WALL where not open to the bay
    (70.0, OUT_X1, 48.0, 67.0),
    (90.0, OUT_X1, 28.0, 67.0),
]
BLIS_CAV = [
    (72.0, CAV_X1, 50.0, 65.0),
    (92.0, CAV_X1, 30.0, 65.0),
]

# Cable entry through the right wall (5-wire bus cable, ~Ø6): slot + zip-tie
# anchor holes in the floor of the wiring bay.
SLOT_Y0, SLOT_Y1, SLOT_Z0 = 45.5, 52.5, 5.0
ZIP_XY = [(110.0, 44.5), (110.0, 53.5)]  # Ø3.5 through-floor
WALLMOUNT_XY = [(20.0, -4.0), (55.0, -4.0), (35.0, 70.0), (85.0, 70.0)]  # Ø4.5

# ------------------------------------------------------------------- helpers
def ky(y):  # KiCad y-down -> STEP y-up
    return -y

def rbox(x0, x1, y0, y1, z0, z1, r=0.0):
    """Box from KiCad-frame extents."""
    ys0, ys1 = ky(y1), ky(y0)
    w, h = x1 - x0, ys1 - ys0
    sk = RectangleRounded(w, h, r) if r > 0 else Rectangle(w, h)
    return extrude(Pos(((x0 + x1) / 2, (ys0 + ys1) / 2, z0)) * sk, z1 - z0)

def cyl(x, y, z0, z1, d):
    return Pos((x, ky(y), (z0 + z1) / 2)) * Cylinder(d / 2, z1 - z0)

# ---------------------------------------------------------------------- base
base = rbox(OUT_X0, OUT_X1, OUT_Y0, OUT_Y1, -FLOOR, RIM_Z, r=3.0)
base -= rbox(CAV_X0, CAV_X1, CAV_Y0, CAV_Y1, 0, RIM_Z + 1, r=2.0)

for x, y in POSTS:                       # cover screw posts
    base += cyl(x, y, 0, RIM_Z, 8.0)
    base -= cyl(x, y, RIM_Z - INSERT_DEPTH, RIM_Z + 0.1, INSERT_D)

for x, y in HOLES:                       # board bosses
    base += cyl(x, y, 0, STANDOFF, 7.0)
    base -= cyl(x, y, STANDOFF - BOSS_INSERT_DEPTH, STANDOFF + 0.1, INSERT_D)

base -= rbox(117.0, OUT_X1 + 1, SLOT_Y0, SLOT_Y1, SLOT_Z0, RIM_Z + 1)  # cable slot
for x, y in ZIP_XY:
    base -= cyl(x, y, -FLOOR - 1, 1, 3.5)
for x, y in WALLMOUNT_XY:
    base -= cyl(x, y, -FLOOR - 1, 1, 4.5)

# --------------------------------------------------------------------- cover
cover = rbox(OUT_X0, OUT_X1, OUT_Y0, OUT_Y1, RIM_Z, RIM_Z + PLATE, r=3.0)

for x0, x1, y0, y1 in BLIS:              # blister shell
    cover += rbox(x0, x1, y0, y1, RIM_Z, BLISTER_CEIL + WALL, r=2.0)
for x0, x1, y0, y1 in BLIS_CAV:          # blister cavity (also opens the plate)
    cover -= rbox(x0, x1, y0, y1, RIM_Z - 0.1, BLISTER_CEIL, r=1.0)

# Alignment skirt inside the base walls (0.25 clearance per side)
sk_o = rbox(CAV_X0 + 0.25, CAV_X1 - 0.25, CAV_Y0 + 0.25, CAV_Y1 - 0.25,
            RIM_Z - 3.0, RIM_Z, r=1.8)
sk_i = rbox(CAV_X0 + 1.45, CAV_X1 - 1.45, CAV_Y0 + 1.45, CAV_Y1 - 1.45,
            RIM_Z - 3.1, RIM_Z + 0.1, r=1.2)
skirt = sk_o - sk_i
for x, y in POSTS:                       # clear the posts
    skirt -= cyl(x, y, RIM_Z - 3.2, RIM_Z + 0.1, 9.0)
skirt -= rbox(114.0, OUT_X1, 42.0, 56.0, RIM_Z - 3.2, RIM_Z + 0.1)  # cable path
cover += skirt

# Display window with dust lip: lip descends to 0.4 above the display frame tabs
wx0, wx1 = VA_CX - WIN_W / 2, VA_CX + WIN_W / 2
wy0, wy1 = VA_CY - WIN_H / 2, VA_CY + WIN_H / 2
cover += rbox(wx0 - 1.5, wx1 + 1.5, wy0 - 1.5, wy1 + 1.5, LIP_BOT, RIM_Z, r=1.0)
cover -= rbox(wx0, wx1, wy0, wy1, LIP_BOT - 0.1, BLISTER_CEIL + WALL + 1, r=1.0)

# Membrane buttons: a Ø12 x 0.5 dish on the outer face makes the button visible
# and findable; an Ø11 pocket cut from the inside leaves a 0.6 membrane under the
# dish floor; a Ø4 nub reaches down to 0.3 above the PTS645 actuator.
DISH_D, DISH_T = 12.0, 0.5
NUB_BOT = BT + H_SW + NUB_GAP
MEMBRANE_TOP = RIM_Z + PLATE - DISH_T           # dish floor
for x, y in SW_XY:
    cover -= cyl(x, y, MEMBRANE_TOP, RIM_Z + PLATE + 1, DISH_D)
    pocket = cyl(x, y, RIM_Z - 0.1, MEMBRANE_TOP - MEMBRANE_T, 11.0)
    pocket -= cyl(x, y, NUB_BOT, MEMBRANE_TOP - MEMBRANE_T + 0.05, 4.0)
    cover -= pocket

# Button glyphs engraved 0.25 into the dish floors (colour inlay via a print
# colour change on the first layers; the membrane keeps >= 0.35 under a stroke —
# untested, first print decides; the simulator shows the same glyphs as inlays).
from glyphs import glyph_solids
DISH_FLOOR = RIM_Z + PLATE - DISH_T
GLYPH_DEPTH = 0.25
for g, _color in glyph_solids(DISH_FLOOR + 0.01, GLYPH_DEPTH + 0.01):
    cover -= g

for x, y in LED_XY:                      # LED view holes (light pipe later)
    cover -= cyl(x, y, RIM_Z - 0.1, RIM_Z + PLATE + 1, 2.0)

for x, y in POSTS:                       # M3 countersunk cover screws
    cover -= cyl(x, y, RIM_Z - 0.2, RIM_Z + PLATE + 1, 3.4)
    cover -= Pos((x, ky(y), RIM_Z + PLATE - 0.85)) * Cone(1.7, 3.3, 1.7)

# ------------------------------------------------------------------- exports
os.makedirs(STL_DIR, exist_ok=True)
export_step(base, os.path.join(STEP_DIR, "enclosure-base-draft.step"))
export_step(cover, os.path.join(STEP_DIR, "enclosure-cover-draft.step"))
export_stl(base, os.path.join(STL_DIR, "enclosure-base-draft.stl"))
export_stl(cover, os.path.join(STL_DIR, "enclosure-cover-draft.stl"))
print("STEP + STL written.")
WRITE_ASSEMBLY = True

# ------------------------------------------------------- interference check
# Note: Pos() * imported Compound does not survive into booleans (locations on
# nested assemblies are lost) -- move each world-placed solid individually.
pcb_solids = [Pos((0, 0, STANDOFF)) * s for s in import_step(PCB_STEP).solids()]
enc = base + cover
total = 0.0
hits = []
for s in pcb_solids:
    try:
        inter = enc & s
        vol = getattr(inter, "volume", 0.0) or 0.0
    except Exception:
        vol = 0.0
    if vol > 0.001:
        bb = s.bounding_box()
        hits.append((vol, bb))
        total += vol
print(f"Interference: {len(hits)} colliding solids, total {total:.3f} mm^3")
for vol, bb in hits:
    print(f"  {vol:8.3f} mm^3 at X {bb.min.X:.1f}..{bb.max.X:.1f} "
          f"Y {bb.min.Y:.1f}..{bb.max.Y:.1f} Z {bb.min.Z:.1f}..{bb.max.Z:.1f}")
if hits:
    sys.exit(1)

# Cover screws: M3 x 8 countersunk (mechanical/draft/screws.py), head flush with
# the plate top. Checked against the enclosure the same way as the board.
SCREW_STEP = os.path.join(STEP_DIR, "m3x8-countersunk-draft.step")
screws = []
if os.path.exists(SCREW_STEP):
    screw_master = import_step(SCREW_STEP)
    for i, (x, y) in enumerate(POSTS):
        inst = Pos((x, ky(y), RIM_Z + PLATE)) * screw_master.solids()[0]
        inst.label = f"screw-m3x8-{i+1}"
        screws.append(inst)
    s_hits = 0
    for sc in screws:
        try:
            v = getattr(enc & sc, "volume", 0.0) or 0.0
        except Exception:
            v = 0.0
        if v > 0.001:
            s_hits += 1
            bb = sc.bounding_box()
            print(f"  SCREW COLLISION {v:.3f} mm^3 at X {bb.min.X:.1f}..{bb.max.X:.1f} "
                  f"Y {bb.min.Y:.1f}..{bb.max.Y:.1f}")
    print(f"Screws: {len(screws)} placed, collisions {s_hits}")
    if s_hits:
        sys.exit(1)
else:
    print("NOTE: screw STEP missing, run mechanical/draft/screws.py first")

if WRITE_ASSEMBLY:
    base.label, cover.label = "enclosure-base-draft", "enclosure-cover-draft"
    board = Compound(children=list(pcb_solids))
    board.label = "pcb"
    asm = Compound(children=[base, cover, board] + screws)
    asm.label = "vallox-rs485-controller-enclosure-draft"
    export_step(asm, os.path.join(STEP_DIR, "enclosure-assembly-draft.step"))
    print("Assembly STEP written (base + cover + board + screws, one origin).")

# --------------------------------------------------------- clearance report
print("\nClearance table (mm, from parameters and STEP-model heights):")
rows = [
    ("J1 pins below board vs floor", STANDOFF - J1_PIN_BELOW, ">=0.4"),
    ("J1 top vs blister ceiling", BLISTER_CEIL - (BT + H_J1), ">=1.0"),
    ("C1 top vs blister ceiling", BLISTER_CEIL - (BT + H_C1), ">=1.0"),
    ("L1 top (4.5) vs main ceiling", MAIN_CEIL - (BT + 4.5), ">=1.0"),
    ("module top vs main ceiling", MAIN_CEIL - (BT + H_MODULE), ">=1.0"),
    ("actuator vs membrane nub", NUB_GAP, "0.3 by design"),
    ("frame tab vs window lip", LIP_BOT - (BT + H_TAB), ">=0.4"),
    ("glass top vs window lip", LIP_BOT - (BT + H_GLASS), ">=0.4"),
    ("board edge vs left wall", 0 - CAV_X0, ">=0.4"),
    ("antenna overhang vs top wall", -CAV_Y0 - 6.0, ">=2.0 target"),
]
for name, val, req in rows:
    print(f"  {name:34s} {val:6.2f}  (req {req})")

# ------------------------------------------------------------------ renders
def render(shapes, origin, path, scale=1.5):
    vis_all, hid_all = [], []
    comp = Compound(children=[s for s in shapes])
    vis, hid = comp.project_to_viewport(viewport_origin=origin)
    exp = ExportSVG(scale=scale)
    exp.add_layer("hid", line_weight=0.12, line_color=(160, 160, 160))
    exp.add_layer("vis", line_weight=0.4, line_color=(0, 0, 0))
    exp.add_shape(hid, layer="hid")
    exp.add_shape(vis, layer="vis")
    exp.write(path)
    print(f"render: {path}")

if os.environ.get("DRAFT_RENDER"):
    out = os.environ["DRAFT_RENDER"]
    os.makedirs(out, exist_ok=True)
    board_solid = max(pcb_solids, key=lambda s: s.bounding_box().size.X)
    render([base], (60, -250, 260), os.path.join(out, "base-iso.svg"))
    render([cover], (60, -250, -240), os.path.join(out, "cover-inside-iso.svg"))
    render([cover], (60, -250, 300), os.path.join(out, "cover-outside-iso.svg"))
    parts = [base, board_solid, Pos((0, 0, 40)) * cover]
    render(parts, (80, -300, 280), os.path.join(out, "assembly-exploded-iso.svg"))
    disp = [s for s in pcb_solids
            if abs(s.bounding_box().size.X - 51.8) < 0.6
            or abs(s.bounding_box().size.X - 40.8) < 0.6
            or abs(s.bounding_box().size.X - 20.7) < 1.0 and s.bounding_box().size.Z < 0.6]
    render([cover] + disp + [board_solid], (57.7, -33.0, 500),
           os.path.join(out, "assembly-top.svg"))
