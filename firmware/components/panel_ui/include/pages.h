// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Pages are data. A new setting is one row here; the engine in panel_ui.c
// knows DASHBOARD / LIST / EDITOR / INFO and nothing about Vallox.
#ifndef PANEL_PAGES_H
#define PANEL_PAGES_H
#include <stdint.h>
#include "texts.h"

typedef enum {
    PAGE_DASHBOARD, PAGE_MENU, PAGE_FAN_SPEED, PAGE_HEAT_SETPOINT, PAGE_STATUS,
    PAGE_SETTINGS, PAGE_LANGUAGE, PAGE_COUNT
} page_id_t;

typedef enum { PAGE_KIND_DASHBOARD, PAGE_KIND_LIST, PAGE_KIND_EDITOR, PAGE_KIND_INFO } page_kind_t;

// How an editor value maps to a register byte (or to a local setting).
typedef enum { ENC_NONE, ENC_FAN_SPEED, ENC_TEMP_C, ENC_LANG } value_enc_t;

typedef struct { text_key_t label; page_id_t target; } item_t;

typedef struct {
    text_key_t    title;
    page_kind_t   kind;
    const item_t *items;   // LIST
    uint8_t       n;
    uint8_t       reg;     // EDITOR with a register encoding
    value_enc_t   enc;
    int16_t       min, max, step;
} page_t;

extern const page_t pages[PAGE_COUNT];
#endif
