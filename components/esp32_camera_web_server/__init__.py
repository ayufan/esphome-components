import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (CONF_ID, ESP_PLATFORM_ESP32)
from esphome.core import coroutine_with_priority

AUTO_LOAD = ['esp32_camera']

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_STREAM_PORT = 'stream_port'
CONF_SNAPSHOT_CPORT = 'snapshot_port'

esp32_camera_web_server_ns = cg.esphome_ns.namespace('esp32_camera_web_server')
WebServer = esp32_camera_web_server_ns.class_('WebServer', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebServer),
    cv.Optional(CONF_STREAM_PORT, default=8080): cv.port,
    cv.Optional(CONF_SNAPSHOT_CPORT, default=8081): cv.port
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_stream_port(config[CONF_STREAM_PORT]))
    cg.add(var.set_snapshot_port(config[CONF_SNAPSHOT_CPORT]))
    yield cg.register_component(var, config)


