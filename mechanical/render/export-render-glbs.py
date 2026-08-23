# SPDX-License-Identifier: CERN-OHL-S-2.0
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""Export the enclosure draft + populated board as GLBs for the render scene.

Usage: python export-render-glbs.py <output-dir>
Needs the workspace CAD venv (build123d) and mechanical/step/pcb.step
(kicad-cli pcb export step --subst-models --no-dnp). See README.md here.
"""
import os, sys
from build123d import import_step, export_gltf, Pos, Compound

HERE = os.path.dirname(os.path.abspath(__file__))
STEP = os.path.join(HERE, "..", "step")
OUT = sys.argv[1]
STANDOFF = 4.0

base = import_step(os.path.join(STEP, "enclosure-base-draft.step"))
cover = import_step(os.path.join(STEP, "enclosure-cover-draft.step"))
pcb = [Pos((0, 0, STANDOFF)) * s for s in import_step(os.path.join(STEP, "pcb.step")).solids()]

def near(a, b, t=0.6): return abs(a - b) < t

board, display, comps = [], [], []
for s in pcb:
    bb = s.bounding_box()
    sx, sy, sz = bb.size.X, bb.size.Y, bb.size.Z
    if near(sx, 104.0):
        board.append(s)
    elif (near(sx, 51.8) and near(sy, 36.2)) or (near(sx, 40.8) and near(sy, 30.6)) \
         or (near(sx, 20.7, 1.0) and near(sy, 20.6, 1.0) and sz < 0.6):
        display.append(s)
    else:
        comps.append(s)

for name, shapes in [("base", [base]), ("cover", [cover]), ("board", board),
                     ("display", display), ("comps", comps)]:
    c = Compound(children=shapes)
    export_gltf(c, os.path.join(OUT, name + ".glb"), binary=True,
                linear_deflection=0.05, angular_deflection=0.3)
    print(name, len(shapes), "solids")
