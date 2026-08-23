# SPDX-License-Identifier: CERN-OHL-S-2.0
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""Export the draft enclosure (base + cover + four screws) as one GLB for the
browser simulator.

The simulator's board.glb has its origin at the board (kicad-cli, user origin
0,0), while the enclosure STEPs use the enclosure frame where the inner floor
is z=0 and the board sits on 4.0 mm standoffs. The -4.0 mm shift below moves
the enclosure into the board frame so both models land aligned in the scene.

Output: mechanical/glb/enclosure.glb (committed: CI has no CAD kernel, and the
file only changes when the enclosure STEPs change).
"""

import os
from build123d import Pos, import_step, export_gltf, Compound

HERE = os.path.dirname(os.path.abspath(__file__))
STEP = os.path.join(HERE, "..", "step")
OUT = os.path.join(HERE, "..", "glb", "enclosure.glb")

STANDOFF = 4.0  # board bottom above the enclosure floor, see enclosure.py
POSTS = [(2.4, -5.0), (2.4, 71.0), (115.0, 71.0), (115.5, 18.0)]
RIM_Z_PLUS_PLATE = 11.51 + 2.5  # cover plate top = screw head seat

parts = []
for name in ("enclosure-base-draft", "enclosure-cover-draft"):
    s = import_step(os.path.join(STEP, name + ".step")).solids()[0]
    parts.append(Pos((0, 0, -STANDOFF)) * s)

screw = import_step(os.path.join(STEP, "m3x8-countersunk-draft.step")).solids()[0]
for x, y in POSTS:
    parts.append(Pos((x, -y, RIM_Z_PLUS_PLATE - STANDOFF)) * screw)

# Colored glyph inlays filling the cover engravings (the print's colour-change
# layers); same geometry source as the cut, shifted into the board frame.
from build123d import Color
from glyphs import glyph_solids
DISH_FLOOR = RIM_Z_PLUS_PLATE - 0.5
for g, color in glyph_solids(DISH_FLOOR - STANDOFF, 0.25):
    g.color = Color(color)
    parts.append(g)

export_gltf(Compound(children=parts), OUT, binary=True,
            linear_deflection=0.05, angular_deflection=0.3)
print(f"{OUT}: {os.path.getsize(OUT)} bytes, {len(parts)} parts")
