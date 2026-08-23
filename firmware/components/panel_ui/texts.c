// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "texts.h"

static lang_t s_lang = LANG_EN;

const char *text_get(text_key_t key)
{
    if ((int)key < 0 || key >= TXT_COUNT) return "?";
    const char *s = (s_lang == LANG_FI) ? texts_fi[key] : texts_en[key];
    return s ? s : "?";
}

void text_set_lang(lang_t lang) { s_lang = (lang == LANG_FI) ? LANG_FI : LANG_EN; }
lang_t text_lang(void) { return s_lang; }
