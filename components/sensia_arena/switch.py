"""Sensia Arena switch platform."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_TYPE

from . import HUB_SCHEMA, register_child, sensia_ns

SwitchKind = sensia_ns.enum("SwitchKind", is_class=True)

DEPENDENCIES = ["sensia_arena"]
SensiaSwitch = sensia_ns.class_("SensiaSwitch", switch.Switch, cg.Component)
TYPES = {
    key: getattr(SwitchKind, key)
    for key in [
        "PAIRING_REPLIES",
        "MAINTENANCE",
        "AUTO_LID",
        "DEODORIZER",
        "BLUE_LED",
        "SOUND",
        "PREWASH",
        "AUTO_FLUSH",
        "POWER_SAVE",
        "HEATER_OFF",
        "FREEZE",
    ]
}


def prepare_restore_mode(config):
    config = dict(config)
    kind = str(config.get(CONF_TYPE, "")).upper()
    expected = {
        "PAIRING_REPLIES": "RESTORE_DEFAULT_OFF",
        "MAINTENANCE": "ALWAYS_OFF",
    }.get(kind, "DISABLED")
    if "restore_mode" in config and str(config["restore_mode"]).upper() != expected:
        raise cv.Invalid(f"{kind} requires restore_mode: {expected}")
    config["restore_mode"] = expected
    return config


CONFIG_SCHEMA = cv.All(
    prepare_restore_mode,
    switch.switch_schema(SensiaSwitch, block_inverted=True)
    .extend(HUB_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.one_of(*TYPES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    await cg.register_component(var, config)
    await register_child(var, config, listener=True)
    cg.add(var.set_kind(TYPES[config[CONF_TYPE]]))
