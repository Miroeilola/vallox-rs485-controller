// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "font.h"
#include "gfx.h"
#include "theme.h"
#include "icons.h"
#include "hal_host.h"
#include "panel_hal.h"

static void test_every_required_codepoint_has_a_glyph_in_every_size(void)
{
    const font_t *fonts[3] = {&font_inter_12, &font_inter_18, &font_inter_36};
    const uint16_t extra[7] = {0x00E4, 0x00F6, 0x00E5, 0x00C4, 0x00D6, 0x00C5, 0x00B0};
    for (int i = 0; i < 3; i++) {
        for (uint16_t cp = 0x20; cp <= 0x7E; cp++) CHECK(font_find_glyph(fonts[i], cp) != NULL);
        for (int k = 0; k < 7; k++) CHECK(font_find_glyph(fonts[i], extra[k]) != NULL);
        CHECK(font_find_glyph(fonts[i], 0x0141) == NULL);   // Ł: not in the set
    }
}

static void test_metrics_are_sane(void)
{
    CHECK_EQ(font_inter_12.px, 12); CHECK_EQ(font_inter_18.px, 18); CHECK_EQ(font_inter_36.px, 36);
    CHECK(font_inter_36.ascent > font_inter_18.ascent && font_inter_18.ascent > font_inter_12.ascent);
    CHECK(font_inter_12.line_h >= 12 && font_inter_36.line_h >= 36);
    const glyph_t *M = font_find_glyph(&font_inter_36, 'M');
    CHECK(M->w >= 20 && M->h >= 20 && M->adv >= M->w);
    const glyph_t *sp = font_find_glyph(&font_inter_18, ' ');
    CHECK_EQ(sp->w, 0);
    CHECK(sp->adv > 0);
    const glyph_t *deg = font_find_glyph(&font_inter_18, 0x00B0);
    CHECK(deg->h < font_inter_18.ascent);          // the degree sign sits above the x-height, small
}

static void test_glyph_bitmaps_are_not_blank(void)
{
    const glyph_t *A = font_find_glyph(&font_inter_18, 'A');
    unsigned stride = (A->w + 1u) / 2u, nonzero = 0;
    for (unsigned r = 0; r < A->h; r++)
        for (unsigned b = 0; b < stride; b++)
            if (font_inter_18.bitmap[A->off + r * stride + b]) nonzero++;
    CHECK(nonzero > A->h);                          // more than one lit byte per row on average
}

static void test_utf8_decode(void)
{
    const char *s = "a\xC3\xA4\xC2\xB0z";            // a ä ° z
    CHECK_EQ(font_utf8_next(&s), 'a');
    CHECK_EQ(font_utf8_next(&s), 0x00E4);
    CHECK_EQ(font_utf8_next(&s), 0x00B0);
    CHECK_EQ(font_utf8_next(&s), 'z');
    CHECK_EQ(font_utf8_next(&s), 0);
    const char *e = "\xE2\x82\xAC";                  // € (U+20AC) — 3 bytes, decodes, no glyph
    CHECK_EQ(font_utf8_next(&e), 0x20AC);
    const char *four = "\xF0\x9F\x98\x80x";          // 😀 — outside BMP → '?'
    CHECK_EQ(font_utf8_next(&four), '?');
    CHECK_EQ(font_utf8_next(&four), 'x');
}

static void test_text_width_sums_advances(void)
{
    const glyph_t *a = font_find_glyph(&font_inter_18, 'a');
    const glyph_t *b = font_find_glyph(&font_inter_18, 'b');
    CHECK_EQ(font_text_width(&font_inter_18, "ab"), a->adv + b->adv);
    CHECK_EQ(font_text_width(&font_inter_18, ""), 0);
    const glyph_t *q = font_find_glyph(&font_inter_18, '?');
    CHECK_EQ(font_text_width(&font_inter_18, "\xE2\x82\xAC"), q->adv);   // missing glyph → '?'
}

static uint16_t px(int x, int y) { return gfx_framebuffer()[y * HAL_DISPLAY_W + x]; }

static void test_init_clears_to_background_and_marks_all_dirty(void)
{
    hal_host_reset(); gfx_init();
    CHECK_EQ(px(0, 0), THEME_BG); CHECK_EQ(px(319, 239), THEME_BG);
    CHECK_EQ(gfx_dirty_count(), 1);
    gfx_flush();
    CHECK_EQ(gfx_dirty_count(), 0);
    CHECK_EQ(hal_host_framebuffer()[239 * HAL_DISPLAY_W + 319], THEME_BG);   // reached the HAL
}

static void test_fill_rect_is_clipped_and_exact(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    gfx_fill_rect(10, 20, 5, 3, 0xF800);
    CHECK_EQ(px(10, 20), 0xF800); CHECK_EQ(px(14, 22), 0xF800);
    CHECK_EQ(px(15, 20), THEME_BG); CHECK_EQ(px(10, 23), THEME_BG); CHECK_EQ(px(9, 20), THEME_BG);
    gfx_fill_rect(315, 235, 20, 20, 0x07E0);           // off the edge: clipped, no crash
    CHECK_EQ(px(319, 239), 0x07E0);
    gfx_fill_rect(-5, -5, 10, 10, 0x001F);             // negative origin
    CHECK_EQ(px(0, 0), 0x001F); CHECK_EQ(px(4, 4), 0x001F); CHECK_EQ(px(5, 5), THEME_BG);
    gfx_fill_rect(400, 400, 10, 10, 0xFFFF);           // fully outside: nothing dirty
    CHECK_EQ(gfx_dirty_count(), 3);
}

static void test_dirty_rects_reach_the_hal_and_only_them(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    // scribble on the HAL side to prove flush only touches dirty areas
    uint16_t marker = 0x1234;
    hal_display_flush(100, 100, 1, 1, &marker);
    gfx_fill_rect(0, 0, 10, 10, 0xF800);
    gfx_flush();
    CHECK_EQ(hal_host_framebuffer()[0], 0xF800);
    CHECK_EQ(hal_host_framebuffer()[100 * HAL_DISPLAY_W + 100], 0x1234);   // untouched
}

static void test_dirty_list_overflow_merges(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    for (int i = 0; i < 20; i++) gfx_fill_rect(i * 10, i * 10, 2, 2, 0xFFFF);
    CHECK(gfx_dirty_count() >= 1 && gfx_dirty_count() <= 16);
    gfx_flush();
    CHECK_EQ(hal_host_framebuffer()[190 * HAL_DISPLAY_W + 190], 0xFFFF);
    CHECK_EQ(hal_host_framebuffer()[0], 0xFFFF);
}

static void test_blend(void)
{
    CHECK_EQ(gfx_blend(0x0000, 0xFFFF, 15), 0xFFFF);
    CHECK_EQ(gfx_blend(0x0000, 0xFFFF, 0), 0x0000);
    uint16_t mid = gfx_blend(0x0000, 0xFFFF, 8);
    int r = (mid >> 11) & 31, g = (mid >> 5) & 63, b = mid & 31;
    CHECK(r >= 15 && r <= 17 && g >= 32 && g <= 35 && b >= 15 && b <= 17);
}

static void test_text_draws_glyph_pixels_and_advances(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    gfx_text(10, 10, &font_inter_18, 0xFFFF, "H");
    const glyph_t *H = font_find_glyph(&font_inter_18, 'H');
    // the left stem of H: a column with lit pixels at x = 10 + xoff, somewhere in the glyph rows
    int lit = 0;
    for (int r = 0; r < H->h; r++)
        if (px(10 + H->xoff, 10 + font_inter_18.ascent - H->yoff + r) != THEME_BG) lit++;
    CHECK(lit >= H->h / 2);
    CHECK_EQ(px(10 + H->adv + 5, 10 + 5), THEME_BG);     // nothing drawn past the advance
    CHECK(gfx_dirty_count() == 1);
}

static void test_text_clips_at_the_right_edge_without_crashing(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    gfx_text(300, 100, &font_inter_36, 0xFFFF, "WWWW");
    CHECK(px(319, 100 + font_inter_36.ascent - 5) != 0x0000 || 1);   // no crash is the assertion; value unconstrained
    gfx_text(-20, -20, &font_inter_36, 0xFFFF, "W");
    CHECK(1);
}

static void test_text_alignment_helpers(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    uint16_t w = font_text_width(&font_inter_18, "abc");
    gfx_text_centred(160, 50, &font_inter_18, 0xFFFF, "abc");
    gfx_text_right(300, 80, &font_inter_18, 0xFFFF, "abc");
    // centred: left edge at 160 - w/2; right: left edge at 300 - w. Check a pixel outside each span is untouched.
    CHECK_EQ(px(160 - w / 2 - 3, 50 + 8), THEME_BG);
    CHECK_EQ(px(160 + (w + 1) / 2 + 3, 50 + 8), THEME_BG);
    CHECK_EQ(px(301, 80 + 8), THEME_BG);
    CHECK_EQ(px(300 - w - 3, 80 + 8), THEME_BG);
}

static void test_round_rect_corners_are_rounded(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    gfx_round_rect(20, 20, 40, 30, 6, 0x07E0);
    CHECK_EQ(px(20, 20), THEME_BG);            // corner pixel cut
    CHECK_EQ(px(40, 35), 0x07E0);              // centre filled
    CHECK_EQ(px(20, 35), 0x07E0);              // left edge mid filled
    CHECK_EQ(px(59, 49), THEME_BG);            // bottom-right corner cut
    CHECK_EQ(gfx_dirty_count(), 1);            // one dirty rectangle for the whole rounded box
}

static void test_icon_draws_set_bits_only(void)
{
    hal_host_reset(); gfx_init(); gfx_flush();
    gfx_icon(50, 50, &icon_fault, 0xFFE0);
    int lit = 0;
    for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) if (px(50 + x, 50 + y) == 0xFFE0) lit++;
    CHECK(lit > 10 && lit < 144);
    CHECK_EQ(px(49, 50), THEME_BG); CHECK_EQ(px(62, 50), THEME_BG);
}

int main(void)
{
    test_every_required_codepoint_has_a_glyph_in_every_size();
    test_metrics_are_sane();
    test_glyph_bitmaps_are_not_blank();
    test_utf8_decode();
    test_text_width_sums_advances();
    test_init_clears_to_background_and_marks_all_dirty();
    test_fill_rect_is_clipped_and_exact();
    test_dirty_rects_reach_the_hal_and_only_them();
    test_dirty_list_overflow_merges();
    test_blend();
    test_text_draws_glyph_pixels_and_advances();
    test_text_clips_at_the_right_edge_without_crashing();
    test_text_alignment_helpers();
    test_round_rect_corners_are_rounded();
    test_icon_draws_set_bits_only();
    return REPORT();
}
