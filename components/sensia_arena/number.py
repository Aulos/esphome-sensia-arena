"""Local wash preferences, applied only on an explicit wash/dryer command."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_TYPE

from . import HUB_SCHEMA, Preference, register_child, sensia_ns

DEPENDENCIES = ["sensia_arena"]
SensiaNumber = sensia_ns.class_("SensiaNumber", number.Number, cg.Component)
TYPES = {
    "PRESSURE": (1, 6, 3),
    "WATER": (0, 5, 1),
    "WIDTH": (1, 3, 1),
    "POSITION": (1, 5, 3),
    "MASSAGE": (0, 5, 0),
    "DRYER": (0, 2, 1),
}
CONFIG_SCHEMA = (
    number.number_schema(SensiaNumber)
    .extend(HUB_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.one_of(*TYPES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    low, high, initial = TYPES[config[CONF_TYPE]]
    await number.register_number(var, config, min_value=low, max_value=high, step=1)
    await cg.register_component(var, config)
    await register_child(var, config)
    cg.add(var.set_kind(getattr(Preference, config[CONF_TYPE])))
    cg.add(var.set_initial_value(initial))
