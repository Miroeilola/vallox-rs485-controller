// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Every string the panel shows, behind one key enum. Two tables, one per
// language; a test fails when a key is missing or uses a glyph the fonts lack.
#ifndef PANEL_TEXTS_H
#define PANEL_TEXTS_H

typedef enum { LANG_EN = 0, LANG_FI = 1, LANG_COUNT } lang_t;

typedef enum {
    TXT_DASHBOARD, TXT_MENU, TXT_FAN_SPEED, TXT_HEAT_SETPOINT, TXT_STATUS, TXT_SETTINGS,
    TXT_LANGUAGE, TXT_ENGLISH, TXT_FINNISH,
    TXT_OUTDOOR, TXT_SUPPLY, TXT_EXTRACT, TXT_EXHAUST,
    TXT_HEATER_ON, TXT_BYPASS, TXT_BOOST_MIN, TXT_NO_BUS,
    TXT_FAULT, TXT_NO_FAULT, TXT_SERVICE_IN, TXT_SERVICE_NOW, TXT_READ_ONLY,
    TXT_FACTORY_RESET, TXT_FIRMWARE, TXT_BUS, TXT_OK_WORD,
    TXT_BTN_MINUS, TXT_BTN_PLUS, TXT_BTN_OK, TXT_BTN_BACK, TXT_BTN_SAVE, TXT_BTN_BACK_WORD, TXT_BTN_UP, TXT_BTN_DOWN,
    TXT_UNIT_C, TXT_STALE, TXT_SPEED_OF,
    TXT_FAULT_SUPPLY_SENSOR, TXT_FAULT_CO2_ALARM, TXT_FAULT_OUTDOOR_SENSOR, TXT_FAULT_EXTRACT_SENSOR,
    TXT_FAULT_WATER_COIL_FROST, TXT_FAULT_EXHAUST_SENSOR, TXT_FAULT_UNKNOWN,
    TXT_COUNT
} text_key_t;

extern const char *const texts_en[TXT_COUNT];
extern const char *const texts_fi[TXT_COUNT];

const char *text_get(text_key_t key);   // current language; a bad key returns "?"
void text_set_lang(lang_t lang);        // out of range → LANG_EN
lang_t text_lang(void);
#endif
