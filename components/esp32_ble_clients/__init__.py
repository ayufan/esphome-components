import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS
from esphome.core import coroutine

DEPENDENCIES = ['esp32']
CONFLICTS_WITH = ['esp32_ble_tracker', 'esp32_ble_beacon']
