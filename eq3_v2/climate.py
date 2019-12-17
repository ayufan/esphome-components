import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, time
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_MAC_ADDRESS, \
    ESP_PLATFORM_ESP32, \
    UNIT_PERCENT, ICON_PERCENT

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CONFLICTS_WITH = ['eq3_v1', 'esp32_ble_tracker']
DEPENDENCIES = ['time']
AUTO_LOAD = ['sensor', 'esp32_ble_clients']

CONF_VALVE = 'valve'
CONF_PIN = 'pin'
CONF_TEMP = 'temperature_sensor'

EQ3Climate = cg.global_ns.class_('EQ3Climate', climate.Climate, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(EQ3Climate),
    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_VALVE): sensor.sensor_schema(UNIT_PERCENT, ICON_PERCENT, 0),
    cv.Optional(CONF_PIN): cv.string,
    cv.Optional(CONF_TEMP): cv.use_id(sensor.Sensor)
}).extend(cv.polling_component_schema('4h')))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    if CONF_TEMP in config:
        sens = yield cg.get_variable(config[CONF_TEMP])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_VALVE in config:
        sens = yield sensor.new_sensor(config[CONF_VALVE])
        cg.add(var.set_valve(sens))
