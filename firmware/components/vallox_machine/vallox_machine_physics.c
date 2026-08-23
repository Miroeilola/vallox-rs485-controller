// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Coarse thermal model: enough for the readings to move the right way when
// the fan speed, the outdoor temperature or the heater setpoint change. First
// order lags, no heat capacity of the house, no defrost cycle — see README.
#include "vallox_machine.h"

#define FROST_LIMIT_C (-2.0f)
#define MONTH_MS (30u * 24u * 3600u * 1000u)

static float lag(float x, float target, float alpha) { return x + alpha * (target - x); }

void vlx_machine_physics_step(vlx_machine_t *m, float dt_s)
{
    int speed = vlx_fan_speed_from_raw(m->regs[VLX_REG_FAN_SPEED]);
    if (speed == VLX_FAN_SPEED_INVALID) speed = 1;
    float tau = m->p.tau_s / (float)speed;
    float alpha = dt_s * m->p.time_scale / tau;
    if (alpha > 1.0f) alpha = 1.0f;

    m->t_extract = lag(m->t_extract, m->p.t_indoor, alpha);
    float recovered = m->p.t_outdoor + m->p.efficiency * (m->t_extract - m->p.t_outdoor);
    float sp = (float)vlx_temp_table(m->regs[VLX_REG_HEAT_SETPOINT]);
    bool heater_on = recovered < sp;
    m->t_supply = lag(m->t_supply, heater_on ? sp : recovered, alpha);
    float exhaust_target = m->t_extract - m->p.efficiency * (m->t_extract - m->p.t_outdoor);
    m->t_exhaust = lag(m->t_exhaust, exhaust_target, alpha);

    // frost protection with hysteresis (0xB2 is in degrees: implementations-class claim)
    float hyst = (float)vlx_temp_table(m->regs[VLX_REG_DEFROST_HYSTERESIS]);
    if (hyst < 1.0f) hyst = 1.0f;
    if (m->t_exhaust < FROST_LIMIT_C) m->regs[VLX_REG_SUPPLY_FAN_STOP] = 1;
    else if (m->t_exhaust > FROST_LIMIT_C + hyst) m->regs[VLX_REG_SUPPLY_FAN_STOP] = 0;

    // service counter
    m->service_elapsed_ms += (uint32_t)(dt_s * m->p.time_scale * 1000.0f);
    while (m->service_elapsed_ms >= MONTH_MS) {
        m->service_elapsed_ms -= MONTH_MS;
        if (m->regs[VLX_REG_SERVICE_MONTHS_LEFT] > 0) m->regs[VLX_REG_SERVICE_MONTHS_LEFT]--;
    }

    // publish through the NTC table so the panel decodes real raw bytes
    m->regs[VLX_REG_TEMP_OUTDOOR] = vlx_temp_to_raw((int16_t)(m->p.t_outdoor + (m->p.t_outdoor >= 0 ? 0.5f : -0.5f)));
    m->regs[VLX_REG_TEMP_EXTRACT] = vlx_temp_to_raw((int16_t)(m->t_extract + 0.5f));
    m->regs[VLX_REG_TEMP_SUPPLY]  = vlx_temp_to_raw((int16_t)(m->t_supply  + (m->t_supply  >= 0 ? 0.5f : -0.5f)));
    m->regs[VLX_REG_TEMP_EXHAUST] = vlx_temp_to_raw((int16_t)(m->t_exhaust + (m->t_exhaust >= 0 ? 0.5f : -0.5f)));
    if (heater_on) m->regs[VLX_REG_STATUS] |= VLX_STATUS_HEATING;
    else           m->regs[VLX_REG_STATUS] &= (uint8_t)~VLX_STATUS_HEATING;
}
