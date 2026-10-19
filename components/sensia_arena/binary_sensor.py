"""Sensia Arena binary_sensor platform."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_TYPE

from . import HUB_SCHEMA, register_child, sensia_ns

BinaryKind = sensia_ns.enum("BinaryKind", is_class=True)

DEPENDENCIES = ["sensia_arena"]
SensiaBinarySensor = sensia_ns.class_("SensiaBinarySensor", binary_sensor.BinarySensor)
TYPES = {
    key: getattr(BinaryKind, key)
    for key in [
        "CONNECTION",
        "OCCUPIED",
        "DESCALING_RECOMMENDED",
        "DESCALING_ACTIVE",
        "DESCALING_ERROR",
        "PENDING",
        "MOTORIZED_LID_SUPPORTED",
        "SEAT_HEATING_SUPPORTED",
        "DESCALING_SUPPORTED",
        "ION_SUPPORTED",
    ]
}
CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(SensiaBinarySensor)
    .extend(HUB_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TYPES, upper=True),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    await register_child(var, config, listener=True)
    cg.add(var.set_kind(config[CONF_TYPE]))
