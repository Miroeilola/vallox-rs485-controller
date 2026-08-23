// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "font.h"

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

int main(void)
{
    test_every_required_codepoint_has_a_glyph_in_every_size();
    test_metrics_are_sane();
    test_glyph_bitmaps_are_not_blank();
    test_utf8_decode();
    test_text_width_sums_advances();
    return REPORT();
}
