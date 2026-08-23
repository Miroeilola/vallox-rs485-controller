// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "gfx.h"
#include "panel_hal.h"
#include "theme.h"

#define W HAL_DISPLAY_W
#define H HAL_DISPLAY_H
#define DIRTY_MAX 16

static uint16_t s_fb[W * H];
typedef struct { int16_t x0, y0, x1, y1; } rect_t;   // inclusive-exclusive: [x0,x1) × [y0,y1)
static rect_t s_dirty[DIRTY_MAX];
static int s_ndirty;

static void mark_dirty(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    if (s_ndirty == DIRTY_MAX) {              // merge everything into one bounding box
        rect_t b = s_dirty[0];
        for (int i = 1; i < s_ndirty; i++) {
            if (s_dirty[i].x0 < b.x0) b.x0 = s_dirty[i].x0;
            if (s_dirty[i].y0 < b.y0) b.y0 = s_dirty[i].y0;
            if (s_dirty[i].x1 > b.x1) b.x1 = s_dirty[i].x1;
            if (s_dirty[i].y1 > b.y1) b.y1 = s_dirty[i].y1;
        }
        s_dirty[0] = b; s_ndirty = 1;
    }
    s_dirty[s_ndirty].x0 = (int16_t)x0; s_dirty[s_ndirty].y0 = (int16_t)y0;
    s_dirty[s_ndirty].x1 = (int16_t)x1; s_dirty[s_ndirty].y1 = (int16_t)y1;
    s_ndirty++;
}

static void fill_rect_raw(int x, int y, int w, int h, uint16_t colour)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int r = y0; r < y1; r++) {
        uint16_t *row = &s_fb[r * W];
        for (int c = x0; c < x1; c++) row[c] = colour;
    }
}

void gfx_init(void)
{
    s_ndirty = 0;
    gfx_fill(THEME_BG);
}

void gfx_fill(uint16_t colour)
{
    for (int i = 0; i < W * H; i++) s_fb[i] = colour;
    s_ndirty = 0;
    mark_dirty(0, 0, W, H);
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t colour)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    fill_rect_raw(x, y, w, h, colour);
    mark_dirty(x0, y0, x1, y1);
}

void gfx_hline(int x, int y, int w, uint16_t colour) { gfx_fill_rect(x, y, w, 1, colour); }
void gfx_vline(int x, int y, int h, uint16_t colour) { gfx_fill_rect(x, y, 1, h, colour); }

void gfx_round_rect(int x, int y, int w, int h, int r, uint16_t colour)
{
    if (r > 8) r = 8;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) { gfx_fill_rect(x, y, w, h, colour); return; }
    // middle band and the two side bands between the corners
    fill_rect_raw(x + r, y, w - 2 * r, h, colour);
    // corner rows: for each row inside the corner, the inset is r - sqrt(r^2 - dy^2), integer
    for (int i = 0; i < r; i++) {
        int dy = r - i;                     // 1..r from the straight edge, top row is i = 0
        int dx = 0;
        while ((dx + 1) * (dx + 1) + (dy - 1) * (dy - 1) <= r * r) dx++;   // widest column still inside the circle
        int inset = r - dx;
        fill_rect_raw(x + inset, y + i, r - inset, 1, colour);                 // top-left
        fill_rect_raw(x + w - r, y + i, r - inset, 1, colour);                 // top-right
        fill_rect_raw(x + inset, y + h - 1 - i, r - inset, 1, colour);         // bottom-left
        fill_rect_raw(x + w - r, y + h - 1 - i, r - inset, 1, colour);         // bottom-right
    }
    fill_rect_raw(x, y + r, r, h - 2 * r, colour);             // left band
    fill_rect_raw(x + w - r, y + r, r, h - 2 * r, colour);     // right band
    mark_dirty(x, y, x + w, y + h);
}

uint16_t gfx_blend(uint16_t bg, uint16_t fg, uint8_t a)
{
    if (a >= 15) return fg;
    if (a == 0) return bg;
    int br = (bg >> 11) & 31, bgc = (bg >> 5) & 63, bb = bg & 31;
    int fr = (fg >> 11) & 31, fgc = (fg >> 5) & 63, fb = fg & 31;
    int r = br + ((fr - br) * a + 7) / 15;
    int g = bgc + ((fgc - bgc) * a + 7) / 15;
    int b = bb + ((fb - bb) * a + 7) / 15;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void draw_glyph(int pen_x, int baseline, const font_t *f, const glyph_t *g, uint16_t colour)
{
    if (g->w == 0 || g->h == 0) return;
    int gx = pen_x + g->xoff, gy = baseline - g->yoff;
    int stride = (g->w + 1) / 2;
    int x0 = gx < 0 ? 0 : gx, y0 = gy < 0 ? 0 : gy;
    int x1 = gx + g->w > W ? W : gx + g->w, y1 = gy + g->h > H ? H : gy + g->h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        const uint8_t *row = &f->bitmap[g->off + (uint32_t)(yy - gy) * (uint32_t)stride];
        uint16_t *dst = &s_fb[yy * W];
        for (int xx = x0; xx < x1; xx++) {
            int c = xx - gx;
            uint8_t byte = row[c >> 1];
            uint8_t a = (c & 1) ? (byte & 0x0F) : (byte >> 4);
            if (a) dst[xx] = gfx_blend(dst[xx], colour, a);
        }
    }
    mark_dirty(x0, y0, x1, y1);
}

void gfx_text(int x, int y, const font_t *f, uint16_t colour, const char *utf8)
{
    int pen = x, baseline = y + f->ascent;
    const char *s = utf8;
    for (;;) {
        uint16_t cp = font_utf8_next(&s);
        if (!cp) break;
        const glyph_t *g = font_find_glyph(f, cp);
        if (!g) g = font_find_glyph(f, '?');
        if (!g) continue;
        draw_glyph(pen, baseline, f, g, colour);
        pen += g->adv;
        if (pen >= W) break;
    }
}

void gfx_text_centred(int cx, int y, const font_t *f, uint16_t colour, const char *utf8)
{
    gfx_text(cx - font_text_width(f, utf8) / 2, y, f, colour, utf8);
}

void gfx_text_right(int right_x, int y, const font_t *f, uint16_t colour, const char *utf8)
{
    gfx_text(right_x - font_text_width(f, utf8), y, f, colour, utf8);
}

void gfx_icon(int x, int y, const icon_t *icon, uint16_t colour)
{
    int bytes_per_row = (icon->w + 7) / 8;
    for (int r = 0; r < icon->h; r++) {
        int yy = y + r;
        if (yy < 0 || yy >= H) continue;
        for (int c = 0; c < icon->w; c++) {
            int xx = x + c;
            if (xx < 0 || xx >= W) continue;
            uint8_t byte = icon->rows[r * bytes_per_row + (c >> 3)];
            if (byte & (0x80u >> (c & 7))) s_fb[yy * W + xx] = colour;
        }
    }
    mark_dirty(x, y, x + icon->w, y + icon->h);
}

void gfx_flush(void)
{
    for (int i = 0; i < s_ndirty; i++) {
        rect_t r = s_dirty[i];
        int w = r.x1 - r.x0;
        for (int yy = r.y0; yy < r.y1; yy++)
            hal_display_flush((uint16_t)r.x0, (uint16_t)yy, (uint16_t)w, 1, &s_fb[yy * W + r.x0]);
    }
    s_ndirty = 0;
}

const uint16_t *gfx_framebuffer(void) { return s_fb; }
int gfx_dirty_count(void) { return s_ndirty; }
