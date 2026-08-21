# SPDX-License-Identifier: MIT
"""ESPHome configuration schema for Vallox RS-485 Controller."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@Miroeilola"]

vallox_rs485_controller_ns = cg.esphome_ns.namespace("vallox_rs485_controller")
Component_ = vallox_rs485_controller_ns.class_("ValloxRs485Controller", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(Component_)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
