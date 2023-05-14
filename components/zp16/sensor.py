import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_RX_ONLY,
    CONF_UPDATE_INTERVAL,
    CONF_TVOC,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ICON_RADIATOR,
    UNIT_MILLIGRAMS_PER_CUBIC_METER,
    STATE_CLASS_MEASUREMENT,
)

CONF_TVOC_SCALE = "tvoc_scale"

DEPENDENCIES = ["uart"]

zp16_ns = cg.esphome_ns.namespace("zp16")
ZP16Component = zp16_ns.class_("ZP16Component", uart.UARTDevice, cg.PollingComponent)


def validate_zp16_rx_mode(value):
    if value.get(CONF_RX_ONLY) and CONF_UPDATE_INTERVAL in value:
        # update_interval does not affect anything in rx-only mode, let's warn user about
        # that
        raise cv.Invalid(
            "update_interval has no effect in rx_only mode. Please remove it.",
            path=["update_interval"],
        )
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZP16Component),
            cv.Optional(CONF_RX_ONLY, default=False): cv.boolean,
            cv.Required(CONF_TVOC): sensor.sensor_schema(
              unit_of_measurement=UNIT_MILLIGRAMS_PER_CUBIC_METER,
              icon=ICON_RADIATOR,
              accuracy_decimals=1,
              device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
              state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TVOC_SCALE): sensor.sensor_schema(
              unit_of_measurement=UNIT_MILLIGRAMS_PER_CUBIC_METER,
              icon=ICON_RADIATOR,
              accuracy_decimals=1,
              device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
              state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("0s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    validate_zp16_rx_mode,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_rx_mode_only(config[CONF_RX_ONLY]))

    sens = await sensor.new_sensor(config[CONF_TVOC])
    cg.add(var.set_tvoc(sens))

    sens = await sensor.new_sensor(config[CONF_TVOC_SCALE])
    cg.add(var.set_tvoc_scale(sens))
