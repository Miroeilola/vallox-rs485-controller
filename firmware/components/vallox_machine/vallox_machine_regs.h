// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// The register map the emulator answers for. Source: docs/research/protocol.md
// and vallox_protocol.h. `conf` is the confidence class from the claim table
// there: manufacturer = in Vallox's own document, implementations = agreed by
// the four independent implementations, assumed = reported by one source or
// inferred. A register absent from this table is unknown and gets NO answer.
// Defaults describe a plausible idle machine in a Finnish autumn, not a real one.
#ifndef VALLOX_MACHINE_REGS_H
#define VALLOX_MACHINE_REGS_H
#include "vallox_machine.h"
#include "vallox_protocol.h"

// Raw NTC values for the defaults come from vlx_temp_to_raw() at init time;
// here only the degrees are listed, so this table stays readable.
typedef struct { uint8_t reg; bool writable; int16_t def_c; vlx_conf_t conf; } temp_reg_def_t;

static const vlx_machine_reg_t k_regs[] = {
    // measurements, read only
    { VLX_REG_IO_FAN_RELAYS,      false, 0x04, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_IO_MULTI_1,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_IO_MULTI_2,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAN_SPEED,          false, 0x07, VLX_CONF_MANUFACTURER },   // speed 3
    { VLX_REG_RH_HIGHEST,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_HIGH,           false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_LOW,            false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SENSORS_FITTED, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CURRENT_MA,         false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_SENSOR_1,        false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_SENSOR_2,        false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAULT,              false, 0x00, VLX_CONF_MANUFACTURER },
    { VLX_REG_POST_HEAT_ON_CNT,   false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_POST_HEAT_OFF_CNT,  false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_POST_HEAT_TARGET,   false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    // flags
    { VLX_REG_FLAGS_1, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_2, true,  0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_3, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_4, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_5, false, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FLAGS_6, true,  0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_BOOST_MINUTES, true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    // settings, writable
    { VLX_REG_STATUS,              true, (uint8_t)(VLX_STATUS_POWER | VLX_STATUS_WINTER_MODE), VLX_CONF_MANUFACTURER },   // power on, heat recovery on; CO2/RH control off (no sensors fitted)
    { VLX_REG_FAN_SPEED_MAX,       true, 0xFF, VLX_CONF_IMPLEMENTATIONS }, // 8
    { VLX_REG_SERVICE_INTERVAL,    true, 0x04, VLX_CONF_IMPLEMENTATIONS }, // months
    { VLX_REG_SUPPLY_FAN_STOP,     true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_FAN_SPEED_DEFAULT,   true, 0x07, VLX_CONF_IMPLEMENTATIONS }, // 3
    { VLX_REG_PROGRAM,             true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_SERVICE_MONTHS_LEFT, true, 0x03, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_RH_BASIC_LEVEL,      true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_DC_FAN_SUPPLY,       true, 0x64, VLX_CONF_IMPLEMENTATIONS }, // 100 %
    { VLX_REG_DC_FAN_EXHAUST,      true, 0x64, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SETPOINT_HIGH,   true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_CO2_SETPOINT_LOW,    true, 0x00, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_PROGRAM_2,           true, 0x00, VLX_CONF_IMPLEMENTATIONS },
};

// Temperature registers: the default is in degrees and converted at init.
static const temp_reg_def_t k_temp_regs[] = {
    { VLX_REG_TEMP_OUTDOOR,       false,  5, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_EXHAUST,       false,  9, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_EXTRACT,       false, 21, VLX_CONF_MANUFACTURER },
    { VLX_REG_TEMP_SUPPLY,        false, 17, VLX_CONF_MANUFACTURER },
    { VLX_REG_HEAT_SETPOINT,      true,  18, VLX_CONF_MANUFACTURER },
    { VLX_REG_PREHEAT_SETPOINT,   true,  -6, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_BYPASS_SETPOINT,    true,  10, VLX_CONF_IMPLEMENTATIONS },
    { VLX_REG_DEFROST_HYSTERESIS, true,   3, VLX_CONF_IMPLEMENTATIONS },
};
// Deliberately absent: the legacy temperature set 0x58-0x5C (assumed; which set
// a machine uses is discovered from traffic), 0xC0 (undocumented), anything
// not in vallox_protocol.h. Unknown registers get no answer.
#endif
