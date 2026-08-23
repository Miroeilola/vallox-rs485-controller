// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "texts.h"
#include "font.h"

static void test_every_key_has_a_non_empty_string_in_both_languages(void)
{
    for (int k = 0; k < TXT_COUNT; k++) {
        CHECK(texts_en[k] != NULL && texts_en[k][0] != '\0');
        CHECK(texts_fi[k] != NULL && texts_fi[k][0] != '\0');
    }
}

static void test_every_character_used_has_a_glyph(void)
{
    // the build-time parity check the spec asks for: no text may use a code point the fonts lack
    const char *const *tables[2] = {texts_en, texts_fi};
    for (int t = 0; t < 2; t++)
        for (int k = 0; k < TXT_COUNT; k++) {
            const char *s = tables[t][k];
            for (;;) {
                uint16_t cp = font_utf8_next(&s);
                if (!cp) break;
                if (cp == '%' || cp == 'd') continue;      // placeholder
                CHECK(font_find_glyph(&font_inter_18, cp) != NULL);
            }
        }
}

static void test_language_switch_and_default(void)
{
    text_set_lang(LANG_EN);
    CHECK(strcmp(text_get(TXT_FAN_SPEED), "Fan speed") == 0);
    text_set_lang(LANG_FI);
    CHECK(strcmp(text_get(TXT_FAN_SPEED), "Puhallinnopeus") == 0);
    CHECK_EQ(text_lang(), LANG_FI);
    text_set_lang((lang_t)7);                              // out of range → English
    CHECK_EQ(text_lang(), LANG_EN);
    CHECK(strcmp(text_get(TXT_BOOST_MIN), "Boost %d min") == 0);
    CHECK(text_get((text_key_t)999) != NULL);              // bad key → never NULL
}

int main(void)
{
    test_every_key_has_a_non_empty_string_in_both_languages();
    test_every_character_used_has_a_glyph();
    test_language_switch_and_default();
    return REPORT();
}
