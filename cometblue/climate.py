import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, time
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_MAC_ADDRESS, \
    ESP_PLATFORM_ESP32
    
ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CONFLICTS_WITH = ['esp32_ble_tracker']
DEPENDENCIES = ['time']
AUTO_LOAD = ['sensor', 'esp32_ble_clients']

CONF_TEMPERATURE_OFFSET = 'temperature_offset'
CONF_WINDOW_OPEN_SENSITIVITY = 'window_open_sensitivity'
CONF_WINDOW_OPEN_MINUTES = 'window_open_minutes'
CONF_PIN = 'pin'

CometBlueClimate = cg.global_ns.class_('CometBlueClimate', climate.Climate, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CometBlueClimate),
    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_PIN, default=-1): cv.int_range(min_included=0, max_included=9999999),
    cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.float_range(min_included=-3, max_included=+3),
    cv.Optional(CONF_WINDOW_OPEN_SENSITIVITY, default=4): cv.int_range(min_included=0, max_included=4),
    cv.Optional(CONF_WINDOW_OPEN_MINUTES, default=10): cv.int_range(min_included=0, max_included=60),
}).extend(cv.polling_component_schema('4h')))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(var.set_window_open_config(config[CONF_WINDOW_OPEN_SENSITIVITY],config[CONF_WINDOW_OPEN_MINUTES] ))

    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
