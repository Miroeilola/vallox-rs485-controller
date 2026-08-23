// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// One theme: colours and layout constants. Dark by default — the panel hangs
// under the machine in a plant room. RGB888 in the source, RGB565 in the code.
#ifndef PANEL_THEME_H
#define PANEL_THEME_H
#include <stdint.h>

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

#define THEME_BG          RGB565(0x12, 0x14, 0x17)   // near-black, slightly blue
#define THEME_BAR         RGB565(0x1C, 0x1F, 0x24)   // top and bottom bars
#define THEME_FG          RGB565(0xE6, 0xE6, 0xE6)   // primary text
#define THEME_FG_DIM      RGB565(0x7A, 0x7F, 0x86)   // secondary / stale values
#define THEME_ACCENT      RGB565(0x8B, 0xC3, 0x4A)   // yellow-green, like the power LED
#define THEME_WARN        RGB565(0xFF, 0xB3, 0x00)   // amber
#define THEME_FAULT       RGB565(0xE5, 0x39, 0x35)   // red
#define THEME_SELECT      RGB565(0x2A, 0x33, 0x3D)   // list selection background

#define THEME_TOP_H       24      // top bar y 0..23
#define THEME_BOTTOM_H    32      // bottom bar y 208..239
#define THEME_CONTENT_Y   THEME_TOP_H
#define THEME_CONTENT_H   (240 - THEME_TOP_H - THEME_BOTTOM_H)
#define THEME_BOTTOM_Y    (240 - THEME_BOTTOM_H)
#define THEME_MARGIN      8
#define THEME_ROW_H       28      // list rows

// Bottom-bar label centres, left to right: − + OK ←. Four equal columns for
// now; the active-area origin vs the switches (spec §7) shifts these in S3/S4.
#define THEME_BTN_X0      40
#define THEME_BTN_X1      120
#define THEME_BTN_X2      200
#define THEME_BTN_X3      280

#endif
