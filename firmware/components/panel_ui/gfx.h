// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Framebuffer renderer: RGB565, dirty-rectangle list, fills, lines, rounded
// boxes, 4-bit antialiased text, 1-bit icons. Pushes only dirty rectangles to
// hal_display_flush(). No allocation, no floats.
#ifndef PANEL_GFX_H
#define PANEL_GFX_H
#include <stdint.h>
#include "font.h"
#include "icons.h"

void gfx_init(void);
void gfx_fill(uint16_t colour);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t colour);
void gfx_hline(int x, int y, int w, uint16_t colour);
void gfx_vline(int x, int y, int h, uint16_t colour);
void gfx_round_rect(int x, int y, int w, int h, int r, uint16_t colour);
void gfx_text(int x, int y, const font_t *f, uint16_t colour, const char *utf8);
void gfx_text_centred(int cx, int y, const font_t *f, uint16_t colour, const char *utf8);
void gfx_text_right(int right_x, int y, const font_t *f, uint16_t colour, const char *utf8);
void gfx_icon(int x, int y, const icon_t *icon, uint16_t colour);
uint16_t gfx_blend(uint16_t bg, uint16_t fg, uint8_t alpha4);
void gfx_flush(void);

const uint16_t *gfx_framebuffer(void);
int gfx_dirty_count(void);
#endif
