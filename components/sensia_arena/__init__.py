"""GROHE Sensia Arena BLE peripheral hub."""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import esp32_ble, esp32_ble_server, time
from esphome.const import CONF_ID, CONF_TIME_ID

AUTO_LOAD = ["esp32_ble_server"]
CODEOWNERS = ["@Aulos"]
DEPENDENCIES = ["esp32", "time"]
DOMAIN = "sensia_arena"
CONF_SENSIA_ARENA_ID = "sensia_arena_id"
CONF_SERVER_ID = "ble_server_id"
CONF_INITIAL_SETTINGS = "initial_settings"
CONF_PAIRING_ENABLED = "pairing_enabled"

sensia_ns = cg.esphome_ns.namespace(DOMAIN)
SensiaArena = sensia_ns.class_("SensiaArena", cg.Component)
Action = sensia_ns.enum("Action", is_class=True)
Preference = sensia_ns.enum("Preference", is_class=True)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SensiaArena),
            cv.GenerateID(CONF_SERVER_ID): cv.use_id(esp32_ble_server.BLEServer),
            cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
            cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_PAIRING_ENABLED): cv.boolean,
            cv.Optional(CONF_INITIAL_SETTINGS, default=0x06): cv.All(
                cv.hex_uint8_t, cv.Range(max=0xFE)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    cv.only_with_framework("esp-idf"),
)


def validate_final(config):
    full = fv.full_config.get()
    ble = full["esp32_ble"]
    server = full["esp32_ble_server"]
    if ble.get("name") != "LIXIL":
        raise cv.Invalid("Sensia Arena requires esp32_ble name: LIXIL")
    if ble.get("auth_req_mode") != "bond":
        raise cv.Invalid("Sensia Arena requires esp32_ble auth_req_mode: bond")
    if CONF_PAIRING_ENABLED in config and any(
        item.get("platform") == DOMAIN and item.get("type") == "PAIRING_REPLIES"
        for item in full.get("switch", [])
    ):
        raise cv.Invalid(
            "Use either pairing_enabled or the PAIRING_REPLIES switch, not both"
        )
    if server.get("max_clients") != 1:
        raise cv.Invalid("Sensia Arena supports one BLE client per ESP32")
    for service in server.get("services", []):
        if str(service["uuid"]).lower() == "6e400001-b5a3-f393-e0a9-e50e24dcca9e":
            raise cv.Invalid(
                "Sensia Arena creates its own service; remove the old YAML BLE service"
            )
    if int(config[CONF_INITIAL_SETTINGS]) & 1:
        raise cv.Invalid(
            "initial_settings bit 0 is managed by the protocol; use an even value"
        )
    return config


FINAL_VALIDATE_SCHEMA = validate_final


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    server = await cg.get_variable(config[CONF_SERVER_ID])
    clock = await cg.get_variable(config[CONF_TIME_ID])
    ble = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(var.set_server(server))
    cg.add(var.set_time(clock))
    cg.add(var.set_initial_settings(config[CONF_INITIAL_SETTINGS]))
    cg.add(var.set_pairing_replies(config.get(CONF_PAIRING_ENABLED, False)))
    esp32_ble.register_gap_event_handler(ble, var)
    esp32_ble.register_gatts_event_handler(ble, var)
    esp32_ble.register_ble_status_event_handler(ble, var)


HUB_SCHEMA = cv.Schema({cv.GenerateID(CONF_SENSIA_ARENA_ID): cv.use_id(SensiaArena)})


async def register_child(var, config, listener=False):
    parent = await cg.get_variable(config[CONF_SENSIA_ARENA_ID])
    cg.add(var.set_parent(parent))
    if listener:
        cg.add(parent.add_listener(var))
