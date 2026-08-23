// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include <string.h>
#include "check.h"
#include "texts.h"
#include "font.h"
#include "buttons.h"

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

static void test_ladder_thresholds_with_50_mv_margins(void)
{
    // spec §3.1: none 3300, SW1 0, SW2 430, SW3 819, SW4 1336 mV; thresholds at midpoints
    CHECK_EQ(buttons_from_mv(0), BTN_MINUS);    CHECK_EQ(buttons_from_mv(50), BTN_MINUS);
    CHECK_EQ(buttons_from_mv(430), BTN_PLUS);   CHECK_EQ(buttons_from_mv(380), BTN_PLUS);  CHECK_EQ(buttons_from_mv(480), BTN_PLUS);
    CHECK_EQ(buttons_from_mv(819), BTN_OK);     CHECK_EQ(buttons_from_mv(769), BTN_OK);    CHECK_EQ(buttons_from_mv(869), BTN_OK);
    CHECK_EQ(buttons_from_mv(1336), BTN_BACK);  CHECK_EQ(buttons_from_mv(1286), BTN_BACK); CHECK_EQ(buttons_from_mv(1386), BTN_BACK);
    CHECK_EQ(buttons_from_mv(3300), BTN_NONE);  CHECK_EQ(buttons_from_mv(3250), BTN_NONE); CHECK_EQ(buttons_from_mv(2400), BTN_NONE);
    // exact midpoints belong to the lower button (< threshold)
    CHECK_EQ(buttons_from_mv(214), BTN_MINUS);  CHECK_EQ(buttons_from_mv(215), BTN_PLUS);
    CHECK_EQ(buttons_from_mv(624), BTN_PLUS);   CHECK_EQ(buttons_from_mv(625), BTN_OK);
    CHECK_EQ(buttons_from_mv(1077), BTN_OK);    CHECK_EQ(buttons_from_mv(1078), BTN_BACK);
    CHECK_EQ(buttons_from_mv(2317), BTN_BACK);  CHECK_EQ(buttons_from_mv(2318), BTN_NONE);
}

// drive the state machine with a sequence of (mv, now) samples, collecting events
static int run_seq(buttons_t *b, const uint16_t *mv, int n, uint32_t t0, button_event_t *out, int max)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        button_event_t e = buttons_tick(b, mv[i], t0 + (uint32_t)i * 20u);
        if (e.kind != BEV_NONE && k < max) out[k++] = e;
    }
    return k;
}

static void test_debounce_needs_two_samples_and_press_fires_once(void)
{
    buttons_t b; buttons_init(&b);
    button_event_t ev[8];
    const uint16_t glitch[] = {3300, 430, 3300, 3300, 3300};          // one-sample glitch: nothing
    CHECK_EQ(run_seq(&b, glitch, 5, 0, ev, 8), 0);
    const uint16_t press[] = {430, 430, 430, 430, 3300, 3300};         // real press: one PRESS on the 2nd sample
    int n = run_seq(&b, press, 6, 1000, ev, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(ev[0].kind, BEV_PRESS); CHECK_EQ(ev[0].button, BTN_PLUS);
    CHECK_EQ(buttons_held(&b), BTN_NONE);
}

static void test_repeat_timing(void)
{
    buttons_t b; buttons_init(&b);
    button_event_t ev[32];
    uint16_t mv[60]; for (int i = 0; i < 60; i++) mv[i] = 430;       // held 1180 ms in 20 ms ticks? no: 60*20 = 1200 → cut at 950
    int n = run_seq(&b, mv, 48, 0, ev, 32);                           // 48 ticks = 0..940 ms
    // PRESS at t=20 (2nd sample); REPEAT at press+500=520 (first tick ≥ 520 is 520), 670→680, 820→820, 970→ not reached
    CHECK_EQ(n, 4);
    CHECK_EQ(ev[0].kind, BEV_PRESS);
    CHECK_EQ(ev[1].kind, BEV_REPEAT); CHECK_EQ(ev[2].kind, BEV_REPEAT); CHECK_EQ(ev[3].kind, BEV_REPEAT);
    CHECK(buttons_held_ms(&b, 940) >= 900);
}

static void test_long_press_fires_once_and_stops_repeats(void)
{
    buttons_t b; buttons_init(&b);
    button_event_t ev[64];
    uint16_t mv[100]; for (int i = 0; i < 100; i++) mv[i] = 1336;    // BACK held 2 s
    int n = run_seq(&b, mv, 100, 0, ev, 64);
    int presses = 0, repeats = 0, longs = 0;
    for (int i = 0; i < n; i++) { if (ev[i].kind == BEV_PRESS) presses++; if (ev[i].kind == BEV_REPEAT) repeats++; if (ev[i].kind == BEV_LONG) longs++; }
    CHECK_EQ(presses, 1); CHECK_EQ(longs, 1);
    // repeats only between 520 ms and 1020 ms: 520, 670, 820, 970 → 4, then LONG at 1020, then silence
    CHECK_EQ(repeats, 4);
    CHECK_EQ(ev[n - 1].kind, BEV_LONG);
}

static void test_button_to_button_jump_is_release_and_press(void)
{
    buttons_t b; buttons_init(&b);
    button_event_t ev[8];
    const uint16_t seq[] = {430, 430, 430, 819, 819, 819, 3300, 3300};
    int n = run_seq(&b, seq, 8, 0, ev, 8);
    CHECK_EQ(n, 2);
    CHECK_EQ(ev[0].button, BTN_PLUS); CHECK_EQ(ev[1].button, BTN_OK); CHECK_EQ(ev[1].kind, BEV_PRESS);
}

int main(void)
{
    test_every_key_has_a_non_empty_string_in_both_languages();
    test_every_character_used_has_a_glyph();
    test_language_switch_and_default();
    test_ladder_thresholds_with_50_mv_margins();
    test_debounce_needs_two_samples_and_press_fires_once();
    test_repeat_timing();
    test_long_press_fires_once_and_stops_repeats();
    test_button_to_button_jump_is_release_and_press();
    return REPORT();
}
