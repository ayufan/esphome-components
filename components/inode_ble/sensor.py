import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_ID, CONF_MAC_ADDRESS, \
    UNIT_EMPTY, ICON_EMPTY, ICON_PULSE, \
    UNIT_PERCENT, ICON_BATTERY, \
    UNIT_WATT, ICON_FLASH, \
    ICON_BRIGHTNESS_5, ICON_CURRENT_AC, \
    ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
DEPENDENCIES = ['esp32_ble_tracker']

iNodeMeterSensor = cg.global_ns.class_('iNodeMeterSensor',
    esp32_ble_tracker.ESPBTDeviceListener, cg.Component)

CONF_CONSTANT = 'constant'
CONF_AVG_RAW = 'avg_raw'
CONF_AVG_W = 'avg_w'
CONF_AVG_DM3 = 'avg_dm3'
CONF_TOTAL_RAW = 'total_raw'
CONF_TOTAL_KWH = 'total_kwh'
CONF_TOTAL_DM3 = 'total_dm3'
CONF_BATTERY_LEVEL = 'battery_level'
CONF_LIGHT_LEVEL = 'light_level'

UNIT_KWH = 'kW/h'

UNIT_DM3 = 'dm3'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(iNodeMeterSensor),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_CONSTANT, default=1000): cv.positive_not_null_int,

    cv.Optional(CONF_AVG_RAW): sensor.sensor_schema(UNIT_EMPTY, ICON_PULSE, 0),
    cv.Optional(CONF_AVG_W): sensor.sensor_schema(UNIT_WATT, ICON_CURRENT_AC, 2),
    cv.Optional(CONF_AVG_DM3): sensor.sensor_schema(UNIT_DM3, ICON_EMPTY, 2),

    cv.Optional(CONF_TOTAL_RAW): sensor.sensor_schema(UNIT_EMPTY, ICON_PULSE, 0),
    cv.Optional(CONF_TOTAL_KWH): sensor.sensor_schema(UNIT_KWH, ICON_FLASH, 2),
    cv.Optional(CONF_TOTAL_DM3): sensor.sensor_schema(UNIT_DM3, ICON_EMPTY, 2),

    cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(UNIT_PERCENT, ICON_BATTERY, 0),
    cv.Optional(CONF_LIGHT_LEVEL): sensor.sensor_schema(UNIT_PERCENT, ICON_BRIGHTNESS_5, 0),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_constant(config[CONF_CONSTANT]))

    if CONF_AVG_RAW in config:
        sens = yield sensor.new_sensor(config[CONF_AVG_RAW])
        cg.add(var.set_avg_raw(sens))
    if CONF_AVG_W in config:
        sens = yield sensor.new_sensor(config[CONF_AVG_W])
        cg.add(var.set_avg_w(sens))
    if CONF_AVG_DM3 in config:
        sens = yield sensor.new_sensor(config[CONF_AVG_DM3])
        cg.add(var.set_avg_dm3(sens))

    if CONF_TOTAL_RAW in config:
        sens = yield sensor.new_sensor(config[CONF_TOTAL_RAW])
        cg.add(var.set_total_raw(sens))
    if CONF_TOTAL_KWH in config:
        sens = yield sensor.new_sensor(config[CONF_TOTAL_KWH])
        cg.add(var.set_total_kwh(sens))
    if CONF_TOTAL_DM3 in config:
        sens = yield sensor.new_sensor(config[CONF_TOTAL_DM3])
        cg.add(var.set_total_dm3(sens))

    if CONF_BATTERY_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_LIGHT_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_LIGHT_LEVEL])
        cg.add(var.set_light_level(sens))
