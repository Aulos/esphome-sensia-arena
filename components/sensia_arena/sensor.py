"""Sensia Arena sensor platform."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_TYPE

from . import HUB_SCHEMA, register_child, sensia_ns

SensorKind = sensia_ns.enum("SensorKind", is_class=True)

DEPENDENCIES = ["sensia_arena"]
SensiaSensor = sensia_ns.class_("SensiaSensor", sensor.Sensor)
TYPES = {
    key: getattr(SensorKind, key)
    for key in [
        "MODEL",
        "GRADE",
        "REGION",
        "FLUSH_DELAY",
        "ION_MODE",
        "SEAT_TEMPERATURE_CODE",
        "ABNORMAL_STATUS",
    ]
}
CONFIG_SCHEMA = (
    sensor.sensor_schema(SensiaSensor)
    .extend(HUB_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TYPES, upper=True),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sensor.register_sensor(var, config)

    await register_child(var, config, listener=True)
    cg.add(var.set_kind(config[CONF_TYPE]))
