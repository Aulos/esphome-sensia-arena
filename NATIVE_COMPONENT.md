# Native component refactor

The integration now has a C++ hub and native ESPHome `button`, `number`, `switch`,
`select`, `binary_sensor`, `sensor`, and `text_sensor` platforms. The package is
configuration only: no lambdas, global session, command scripts or manual callback
slot build flags. The protocol encoder uses fixed 20-byte arrays.

The native refactor is installed on the test ESP32. Initial hardware results are
recorded below; earlier wash/dryer confirmations in the README still apply to
the preceding YAML implementation. Do not flash the test fixtures: they contain
dummy Wi-Fi settings and omit your Home Assistant API/OTA credentials.

## Minimal configuration

Use a normal ESP32/ESP-IDF configuration, network configuration and a time source,
then add the following. During local development point `external_components` at
the `components` directory in this checkout. For another machine, first publish
and pin a tested revision; the existing remote main may still contain the older
implementation.

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [sensia_arena]

esp32_ble:
  name: LIXIL
  auth_req_mode: bond
  max_connections: 1

esp32_ble_server:
  max_clients: 1

time:
  - platform: homeassistant
    id: local_time

sensia_arena:
  id: toilet
  time_id: local_time
  initial_settings: 0x06
  pairing_enabled: true

binary_sensor:
  - platform: sensia_arena
    sensia_arena_id: toilet
    type: OCCUPIED
    name: Seat occupied
```

`pairing_enabled` is an explicit opt-in for a setup without a pairing switch.
It defaults false. Do not combine it with the `PAIRING_REPLIES` switch: that switch
restores its prior enablement and defaults off on first installation. Neither mode
automatically erases BLE bonds. As before, enabling replies permits new pairing
and the application handshake. Separating those permissions is future work.

`initial_settings` is the same encoded byte documented in README.md, with bit 0
reserved. It is sent before the first status of each connection and can affect
stored toilet preferences. This refactor does not claim to solve that protocol
limitation. A time component is required; it need not be Home Assistant time.

## Full configuration and migration

`packages/sensia-arena.yaml` exposes the same entity names, explicit IDs,
categories and disabled-by-default flags as the previous package. Local Home
Assistant display-name overrides, including the grouped names, should continue
to refer to those entities. The six local preference numbers use the same entity
preference key and float storage as the old template numbers. Verify restored
values after the first hardware upgrade.

All entities are optional. The `type` values accepted by each platform are listed
in its Python module and demonstrated in `tests/full.yaml`. Buttons are explicit
physical commands. Number edits only change the profile used by the next wash or
dryer command. The select controls seat temperature; the text sensor reports
protocol status. Unsupported variants remain monitoring-only.

The pairing switch always uses `RESTORE_DEFAULT_OFF`; maintenance always resets
OFF. Physical setting switches use `DISABLED` restoration and do not send anything
at boot. Conflicting restore modes and inversion are rejected. Flush controls
still require a physical electronic actuator.

To upgrade a real node, retain its device name, friendly name, ESP32 target,
network, API key, OTA credentials and time zone. Replace the old package in one
step, including its old manually configured BLE service. Do not load both old and
new services, and do not copy the test fixture's device name. Keep a known-good
firmware/configuration available for rollback. Re-test pairing, reboot/bond
restoration, occupancy and one benign setting before wash/dryer tests.

## Transport behavior

The toilet remains the BLE central; the ESP32 advertises a peripheral named
LIXIL. Native code registers its GAP/GATTS callbacks through ESPHome codegen.
The server retains its CCCD callback; the hub observes the descriptor write via
GATTS without replacing that callback.

The hub accepts protocol frames only from its authenticated connection. Replies
wait for a valid clock and notification subscription. Failed handshake sends stay
pending; successful IDF submission clears them. This is local transport acceptance,
not proof the toilet processed a notification. Physical commands are never retried
automatically. Raw pairing identities are no longer logged.

Setting commands wait for a change in the corresponding reported field or a
reconnect, preserving the protection against repeated toggle commands. A lost
acknowledgement can therefore still block further settings until reconnection.
Stop remains available during this wait. There is deliberately no timeout that
blindly permits another toggle against stale state.

Binary states are invalidated on disconnect and numeric diagnostics publish NaN.
Switch/select internal state is marked unknown, but ESPHome's live API may retain
the last displayed value until the next update; use the connection diagnostic.
No arbitrary quiet-period timeout is imposed on an otherwise connected toilet.

## Tests

Run with an environment containing ESPHome and PyYAML:

```sh
python tests/test_config.py
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  test_protocol.cpp -o /tmp/sensia-protocol-test
/tmp/sensia-protocol-test
clang++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I tests/stubs tests/test_hub.cpp components/sensia_arena/sensia_arena.cpp \
  -o /tmp/sensia-hub-test
/tmp/sensia-hub-test
esphome compile tests/minimal.yaml
esphome compile tests/full.yaml
```

The protocol tests retain existing packet vectors and capability/interlock checks.
The hub tests compile the actual hub against small platform test doubles and cover
unauthenticated/wrong-connection writes, pairing permission, subscription and clock
gates, failed-send recovery, pending toggles, occupancy, Stop and reconnect reset.
The doubles do not simulate radio timing or the ESP-IDF stack. Configuration tests
check valid minimal/full setups, invalid configurations and the entity migration
manifest.

## Local validation on 2026-10-19

- Existing protocol tests passed with address and undefined-behavior sanitizers.
- Native hub lifecycle tests passed with the same sanitizers.
- Configuration tests passed, including the legacy entity manifest and invalid
  pairing, BLE, restore-mode, service and entity settings.
- Minimal and full ESP32-C3/ESP-IDF firmware builds succeeded with the installed
  ESPHome 2026.9.1 toolchain.
- Python lint and formatting checks passed.

## Native hardware validation on 2026-10-19

Firmware revision `e2889f7ce314e053f1b169215eb25b1b3462ed12` compiled in the
ESPHome dashboard with 2026.10.0b4 and ESP-IDF 5.5.5, then installed over OTA on
Waveshare ESP32-C3-Zero paired with Sensia Arena 39354SH0. A private full-flash
backup and original configuration were saved before installation.

- Boot and Home Assistant API connection succeeded.
- Existing Bluetooth bond authenticated without pressing the pairing buttons.
  Status reception succeeded after the board was moved beside the toilet.
- Model, grade and region reported 2/2/2.
- Existing grouped entity names and six stored preference values were retained:
  pressure 6, width 3, nozzle position 3, water temperature 5, massage 0, dryer 2.
- User sit/stand test produced occupied at 12:14:32 and unoccupied at 12:14:44
  in Home Assistant history.
- Blue LED was commanded off and then restored on. The reported setting changed
  and the pending-command flag cleared after each command. Physical light output
  was not separately confirmed by the user.

The owner confirmed Open lid, Rear wash followed by Stop, and Dryer followed by
Stop physically work on this native build. USB logs recorded rear wash opcode
`7E`, dryer `7B`, and Stop `F2` after each. Physical confirmation comes from the
owner, not merely from successful packet submission.

The owner then unplugged USB power for five seconds and reconnected the board
beside the toilet without pressing pairing buttons. Home Assistant showed a fresh
BLE connection and `Status received`. All six preference values above remained
unchanged, and pairing replies remained enabled. This confirms recovery after
one power cycle; long-duration reliability has not yet been measured.
Other models, grades, regions and boards are unverified.

### Observed periodic disconnects

Later same-day idle monitoring showed occupancy becoming unknown for 3–8 seconds
roughly every five minutes. Live OTA logs captured the cause at 14:49:09.142:
BLE disconnected with HCI reason `0x13` (remote device terminated connection).
The ESP32 API log stream stayed connected throughout. BLE reconnected at
14:49:13.036, authentication succeeded at 14:49:13.949, and occupancy/status
returned at 14:49:14.564. This was not an ESP32 reboot.

The hub has no five-minute occupancy expiry. Disconnect resets the session,
and binary sensors invalidate their state until a fresh authenticated status
arrives. Retaining occupancy across the gap would show a stale observation;
command interlocks must continue to require fresh session status regardless of
any future display smoothing. The reason the toilet ends its session periodically
is not established by this capture; a toilet idle policy or a missing protocol
keepalive needs further investigation. No keepalive or display grace period has
been added on the basis of this evidence.


## Upstream preparation still required

Split this complete external implementation into hub plus one platform, then
follow-up platform PRs, as described in UPSTREAM_RESEARCH.md. The current combined
change is not intended as one upstream PR. Resolve licensing before submission,
confirm a human maintainer owns the code, add upstream-format architecture compile
fixtures and documentation PRs, and test on current upstream dev. Only the C3
configuration is covered here; other hardware remains unverified.
