"""Sensia Arena button platform."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_TYPE

from . import HUB_SCHEMA, Action, register_child, sensia_ns

DEPENDENCIES = ["sensia_arena"]
SensiaButton = sensia_ns.class_("SensiaButton", button.Button)
TYPES = {
    key: getattr(Action, key)
    for key in [
        "STOP",
        "OPEN_LID",
        "OPEN_SEAT",
        "CLOSE",
        "FLUSH_FULL",
        "FLUSH_SMALL",
        "REAR",
        "GENTLE",
        "FRONT",
        "DRY",
        "CLEAN_NOZZLE",
        "CLEAN_REAR",
        "CLEAN_FRONT",
        "CLEAN_GUARD",
        "ION_CYCLE",
        "DELAY_CYCLE",
        "DESCALE",
        "RETRY_DRAIN",
    ]
}
CONFIG_SCHEMA = (
    button.button_schema(SensiaButton)
    .extend(HUB_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TYPES, upper=True),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)

    await register_child(var, config, listener=False)
    cg.add(var.set_kind(config[CONF_TYPE]))
