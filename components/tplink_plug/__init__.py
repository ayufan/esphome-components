import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, switch
from esphome.const import CONF_ID, CONF_VOLTAGE, CONF_CURRENT, CONF_STATE

tplink_ns = cg.esphome_ns.namespace('tplink')
TplinkComponent = tplink_ns.class_('TplinkComponent', cg.Component)
Plug = tplink_ns.struct('Plug')

DEPENDENCIES = ['json']
AUTO_LOAD = ['json']

CONF_PLUGS = 'plugs'
CONF_TOTAL = 'total'

PLUG_SCHEMA = cv.Schema({
    cv.Required(CONF_STATE): cv.use_id(switch.Switch),
    cv.Optional(CONF_VOLTAGE): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_CURRENT): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_TOTAL): cv.use_id(sensor.Sensor),
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TplinkComponent),
    cv.Optional(CONF_PLUGS): cv.ensure_list(PLUG_SCHEMA),
})

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    for conf in config.get(CONF_PLUGS, []):
        state = yield cg.get_variable(conf[CONF_STATE])

        current = 0
        if CONF_CURRENT in conf:
            current = yield cg.get_variable(conf[CONF_CURRENT])

        voltage = 0
        if CONF_VOLTAGE in conf:
            voltage = yield cg.get_variable(conf[CONF_VOLTAGE])

        total = 0
        if CONF_TOTAL in conf:
            total = yield cg.get_variable(conf[CONF_TOTAL])

        plug = cg.StructInitializer(
            Plug,
            ('current_sensor', current),
            ('voltage_sensor', voltage),
            ('total_sensor', total),
            ('state_switch', state)
        )

        cg.add(var.add_plug(plug))
