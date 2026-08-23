// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "pages.h"
#include "vallox_protocol.h"

static const item_t k_menu[] = {
    { TXT_FAN_SPEED,     PAGE_FAN_SPEED },
    { TXT_HEAT_SETPOINT, PAGE_HEAT_SETPOINT },
    { TXT_STATUS,        PAGE_STATUS },
    { TXT_SETTINGS,      PAGE_SETTINGS },
};

static const item_t k_settings[] = {
    { TXT_LANGUAGE, PAGE_LANGUAGE },
};

const page_t pages[PAGE_COUNT] = {
    [PAGE_DASHBOARD]     = { TXT_DASHBOARD,     PAGE_KIND_DASHBOARD, 0, 0, 0, ENC_NONE, 0, 0, 0 },
    [PAGE_MENU]          = { TXT_MENU,          PAGE_KIND_LIST, k_menu, 4, 0, ENC_NONE, 0, 0, 0 },
    [PAGE_FAN_SPEED]     = { TXT_FAN_SPEED,     PAGE_KIND_EDITOR, 0, 0, VLX_REG_FAN_SPEED, ENC_FAN_SPEED, 1, 8, 1 },
    [PAGE_HEAT_SETPOINT] = { TXT_HEAT_SETPOINT, PAGE_KIND_EDITOR, 0, 0, VLX_REG_HEAT_SETPOINT, ENC_TEMP_C, 10, 25, 1 },
    [PAGE_STATUS]        = { TXT_STATUS,        PAGE_KIND_INFO, 0, 0, 0, ENC_NONE, 0, 0, 0 },
    [PAGE_SETTINGS]      = { TXT_SETTINGS,      PAGE_KIND_LIST, k_settings, 1, 0, ENC_NONE, 0, 0, 0 },
    [PAGE_LANGUAGE]      = { TXT_LANGUAGE,      PAGE_KIND_EDITOR, 0, 0, 0, ENC_LANG, 0, 1, 1 },
};
