"""Reported seat-temperature control."""

import esphome.codegen as cg
from esphome.components import select
from esphome.const import CONF_ID

from . import HUB_SCHEMA, register_child, sensia_ns

DEPENDENCIES = ["sensia_arena"]
SensiaSelect = sensia_ns.class_("SensiaSelect", select.Select)
CONFIG_SCHEMA = select.select_schema(SensiaSelect).extend(HUB_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(
        var, config, options=["Off", "28 °C", "30 °C", "32 °C", "34 °C", "36 °C"]
    )
    await register_child(var, config, listener=True)
