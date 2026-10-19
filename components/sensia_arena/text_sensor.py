"""Protocol status."""

import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import HUB_SCHEMA, register_child, sensia_ns

DEPENDENCIES = ["sensia_arena"]
SensiaTextSensor = sensia_ns.class_("SensiaTextSensor", text_sensor.TextSensor)
CONFIG_SCHEMA = text_sensor.text_sensor_schema(SensiaTextSensor).extend(HUB_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await text_sensor.register_text_sensor(var, config)
    await register_child(var, config, listener=True)
