# SPDX-License-Identifier: CERN-OHL-S-2.0
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
"""Button glyphs (-, +, OK, back) as build123d solids, shared by the cover
engraving (enclosure.py) and the colored inlays in the simulator GLB
(export-enclosure-glb.py). One source so the cut and the fill always match.

Buttons SW1..SW4 map to BTN_MINUS, BTN_PLUS, BTN_OK, BTN_BACK
(firmware/components/panel_ui/include/buttons.h)."""

from build123d import Pos, Rot, Box, Text, extrude, Compound

# (x, y_kicad) of the dish centres, from enclosure.py SW_XY / SW row y=52.0
GLYPH_XY = [(14.5, 52.0), (27.5, 52.0), (40.5, 52.0), (53.5, 52.0)]
GLYPH_COLORS = ["#3f8fe6", "#e87c33", "#5fbf77", "#8d939b"]  # render palette

def _minus():
    return Box(3.6, 1.2, 1.0)

def _plus():
    return Box(3.6, 1.2, 1.0) + Box(1.2, 3.6, 1.0)

def _ok():
    sk = Text("OK", font_size=4.2)
    return extrude(sk, 0.5) + Pos((0, 0, -0.5)) * extrude(sk, 0.5)

def _back():
    # left-pointing chevron with a tail (the UI's return arrow, simplified)
    tail = Pos((0.9, 0, 0)) * Box(2.6, 1.1, 1.0)
    arm1 = Pos((-0.9, 0.75, 0)) * Rot(0, 0, -40) * Box(2.3, 1.1, 1.0)
    arm2 = Pos((-0.9, -0.75, 0)) * Rot(0, 0, 40) * Box(2.3, 1.1, 1.0)
    return tail + arm1 + arm2

def glyph_solids(z_top, depth, ky=lambda y: -y):
    """Glyph prisms whose top face is at z_top, `depth` tall, positioned on the
    dish centres. Returns [(solid, color_hex), ...]."""
    out = []
    for (x, y), color, mk in zip(GLYPH_XY, GLYPH_COLORS, (_minus, _plus, _ok, _back)):
        g = mk()
        bb = g.bounding_box()
        # normalise: centre XY, put the top at z_top
        g = Pos((x - (bb.min.X + bb.max.X) / 2,
                 ky(y) - (bb.min.Y + bb.max.Y) / 2,
                 z_top - bb.max.Z)) * g
        # trim to exactly `depth` tall
        cut = Pos((x, ky(y), z_top - depth - 2.0)) * Box(20, 20, 4.0)
        out.append((g - cut, color))
    return out
