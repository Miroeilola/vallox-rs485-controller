// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "font.h"

const glyph_t *font_find_glyph(const font_t *f, uint16_t cp)
{
    // glyphs are sorted by code point: binary search
    uint16_t lo = 0, hi = f->n;
    while (lo < hi) {
        uint16_t mid = (uint16_t)(lo + (hi - lo) / 2);
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = (uint16_t)(mid + 1); else hi = mid;
    }
    return 0;
}

uint16_t font_utf8_next(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    uint8_t b0 = p[0];
    if (b0 == 0) return 0;
    uint32_t cp; int n;
    if (b0 < 0x80)        { cp = b0;        n = 1; }
    else if (b0 < 0xC0)   { cp = '?';       n = 1; }          // stray continuation byte
    else if (b0 < 0xE0)   { cp = b0 & 0x1F; n = 2; }
    else if (b0 < 0xF0)   { cp = b0 & 0x0F; n = 3; }
    else                  { cp = b0 & 0x07; n = 4; }
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *s += i; return '?'; }  // truncated sequence
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s += n;
    if (cp > 0xFFFF) return '?';
    return (uint16_t)cp;
}

uint16_t font_text_width(const font_t *f, const char *utf8)
{
    uint16_t w = 0;
    const char *s = utf8;
    for (;;) {
        uint16_t cp = font_utf8_next(&s);
        if (!cp) break;
        const glyph_t *g = font_find_glyph(f, cp);
        if (!g) g = font_find_glyph(f, '?');
        if (g) w = (uint16_t)(w + g->adv);
    }
    return w;
}
