import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import (
    CONF_ID, ESP_PLATFORM_ESP32)
from esphome.core import coroutine_with_priority

AUTO_LOAD = ['web_server_base', 'esp32_camera']

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

esp32_camera_web_server_ns = cg.esphome_ns.namespace('esp32_camera_web_server')
WebServer = esp32_camera_web_server_ns.class_('WebServer', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebServer),
    cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
        web_server_base.WebServerBase
    ),
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    paren = yield cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    yield cg.register_component(var, config)


