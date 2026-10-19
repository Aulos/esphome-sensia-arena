# ESPHome Sensia Arena

Experimental ESPHome controls for GROHE Sensia Arena BLE toilets.
Test hardware: 39354SH0 and Waveshare ESP32-C3-Zero.

This implements the BLE peripheral role recovered from Android app 1.4.1.
The toilet connects to the ESP32, which advertises as LIXIL. No phone app is
needed. The board requires USB power and Wi-Fi; no wiring to the toilet.

Pairing and status reception were verified on a 39354SH0 with the
Waveshare ESP32-C3-Zero and ESPHome 2026.10.0b4 on 2026-10-19.
Bluetooth authentication, the application handshake, and normal 20-byte status
packets succeeded. Additional controls are decoded from the app; their hardware
validation is recorded separately below.
No automatic product/model assumption is made from the reported model code.

## Files
- `sensia-arena.yaml`: ESPHome configuration, tested with ESPHome 2026.9.1.
- `packages/sensia-arena.yaml`: Shared controls and BLE configuration.
- `components/sensia_arena/protocol.h`: Status parser, capability checks and command encoder.
- `sensia_protocol.h`: Include for standalone host tests.
- `secrets.example.yaml`: Placeholder credentials; copy locally to `secrets.yaml`.
- `test_protocol.cpp`: Host checks with synthetic packets and a captured status frame.

## Install
1. Copy sensia-arena.yaml into your ESPHome configuration directory.
   ESPHome downloads the package and protocol component from this repository. Copy secrets.example.yaml to secrets.yaml, or merge its keys into
   your existing secrets.yaml. Set your 2.4 GHz Wi-Fi name/password. Generate
   the API key with `openssl rand -base64 32` and the OTA password with
   `openssl rand -hex 24`. Replace both placeholders. Never commit secrets.yaml.
2. Use ESPHome 2026.9.1 or a newer version that supports custom BLE server
   characteristics. Set the timezone substitution if outside Europe/Warsaw.
3. Review initial_settings below before enabling pairing replies.
4. Compile and install over USB with ESPHome Device Builder or:
     esphome run sensia-arena.yaml
   Compile with your own credentials before flashing.
5. Add the device through Home Assistant's ESPHome integration. Use the API key
   from secrets.yaml if prompted. Inspect the ESPHome logs alongside HA entities.

## First connection
The firmware advertises automatically. Use the toilet's remote-control
smartphone-pairing procedure from the GROHE manual for 39354SH0. The ESP32 takes
the phone's role. Hold DRYER + WATER TEMP for two seconds until STOP blinks.
Manufacturer manual:
https://cdn.cloud.grohe.com/Literature/Brochures/EN/INT00009/original/INT00009.pdf

After reviewing initial_settings, enable 'Enable pairing replies' in Home
Assistant, then start pairing on the remote. The switch defaults OFF on a fresh
installation and remembers your choice across reboots. OFF rejects new Bluetooth
pairing requests and disables application replies; it does not erase saved bonds.

Replies require a valid Home Assistant clock and a received 20-byte pairing
packet. ESPHome manages notification subscriptions internally. Do not add a YAML
on_write callback to the 2902 descriptor: in the tested ESPHome version it replaces
the internal subscription callback and silently prevents notifications.

The GAP security callback is registered at boot priority -100, after BLE setup.
The reserved callback slot assumes this configuration is the only GAP-event
consumer; combining it with other BLE components needs a proper external component
that allocates callback slots through ESPHome code generation.

Expected progression:

```text
Waiting for toilet
BLE connected; awaiting packets
Pairing packet received
Status received
```

'Toilet BLE connection' only reports a radio connection, not authenticated pairing.
'Seat occupied' stays unknown until status arrives and becomes unknown on
disconnect. Quiet periods longer than 60 seconds were observed while connected,
so there is no arbitrary status timeout. 'Reported model code' is the
raw 2-bit model field, not a verified product identity.

## Initial settings: the pairing response is not strictly read-only
The recovered opcode 00 carries settings as well as the pairing identity.
Until the first status arrives, the current toilet settings are unknown.
The default initial_settings 0x06 follows the app's startup values:
  auto-wash delay code 0
  heater-off flag 0
  seat-temperature code 1
  auto-wash flag 1
The firmware adds connection bit 0, producing 0x07 on the wire.
These defaults MAY change seat heating or automatic flushing configuration.
That is why replies require explicit opt-in on the first installation.

To match known settings, initial_settings is an encoded byte:
  bits 7..6: auto-wash delay code
  bit 5: seat-heater-off flag
  bits 4..2: seat-temperature code
  bit 1: automatic washing/flushing flag
  bit 0: supplied by firmware
Do not assume the codes are temperatures in degrees. Once a status arrives,
the firmware derives these fields and drainage mode from the received status
instead of continuing to use startup defaults. No schedule configuration is sent.

Enablement is restored after reboot. Initial settings are still used before the
first status of each connection, so configure them to match your preferences.

## Scope and diagnostics
Controls and their limits are described below. Pairing and notification delivery
were verified; seat occupancy transitions were confirmed in Home Assistant.
Effects of all settings have not been verified. A successful compile does not
establish that the toilet will connect or accept this handshake.

If it does not connect, retain logs from startup through the pairing attempt.
If connected but stuck, include RX packets, subscription logs, clock synchronization
and any notification errors. RX pairing packets can contain the toilet's identity;
redact those bytes if posting logs publicly. No phone-app capture is required.

The protocol was recovered from APK 1.4.1 classes
`com.grohe.sensiaarena.bluetooth.BLEManager` and
`com.grohe.sensiaarena.toilet.NToiletInfoManager`.
See [the protocol report](protocol-report.txt) for findings and artifact hashes.
The APK, decompiled application sources and raw REA evidence are not distributed
in this repository. Source line references in the report refer to the local
analysis export, not repository files.

Host test:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
    test_protocol.cpp -o /tmp/sensia-protocol-test
/tmp/sensia-protocol-test
```

References:
https://esphome.io/components/esp32_ble_server/
https://esphome.io/components/esp32_ble/
https://www.waveshare.com/wiki/ESP32-C3-Zero

## Validation (2026-10-16)
ESPHome 2026.9.1 full ESP-IDF build succeeded for seeed_xiao_esp32c3.
Reported static RAM: 135856 bytes (42.3%); application flash: 1197068 bytes (65.2%).
Host protocol checks passed with address and undefined-behavior sanitizers.
These checks used synthetic packets. No hardware was flashed or tested.

## Hardware validation (2026-10-19)
USB HCI capture exposed two blockers: unanswered SMP pairing requests and a
custom CCCD callback replacing ESPHome's notification bookkeeping. Both fixes
were installed over encrypted OTA. Authentication reported success and the toilet
sent normal status frames after the opcode-00 reply. Reported model code was 2.
Host protocol checks, including a captured status frame, passed with address and
undefined-behavior sanitizers. Raw HCI captures are excluded because they contain
Bluetooth key material.

Home Assistant recorded occupied at 10:04:28 and clear at 10:04:33 during the
physical seat test. The former 60-second stale timeout then marked it unknown;
the cleaned configuration removes that timeout.

Clean firmware was installed on ESPHome 2026.10.0b4 (build 10:04:51) and tested
with a USB-triggered reboot. Home Assistant showed pairing replies restored ON,
protocol status received, and occupancy clear at 10:10:26 without restarting
pairing on the remote. Occupancy remained available beyond two minutes.

## Controls and variant detection

The integration reads model, grade and region from every status frame. Region is
an internal protocol field, not Home Assistant's language. These capability rules
come from the Android app's NT screens. Only model 2, grade 2, region 2 has been
connected to physical hardware during development.

| Feature | Model 2 region 1 | Model 2 region 2 | Model 2 region 3 | Model 2 region 4 | Model 3 |
|---|---|---|---|---|---|
| Wash, dryer, flush, nozzle cleaning | Yes | Yes | Yes | Yes | Yes |
| Auto flush, delay, deodorizer, sound | Yes | Yes | Yes | Yes | Yes |
| Motorized lid/seat, automatic lid | Grades 2–3 | Grades 2–3 | Grades 2–3 | Grades 2–3 | Grades 2–3 |
| Blue LED, pre-wash, ion mode | No | Yes | Yes | Yes | No |
| Descaling | No | Yes | No | Yes | No |
| Seat heat, heater auto-off, power saving | No | No | Yes | Yes | No |
| Freeze protection | No | No | Yes | No | No |

Model 2 is named Arena wall and model 3 Arena floor in the APK. Grade codes
1/2/3 correspond to its GRADE_5/GRADE_6/GRADE_8 constants. Regions 3 and 4 are
named China/Taiwan and EU in the app. These codes do not identify a retail SKU.
Models 0/1 use other app paths and are rejected by the BLE UI. Unknown model,
grade or region codes remain available for monitoring but cannot actuate.
The known region range is 1–4; unknown values do not inherit optional features.

ESPHome declares a fixed entity list. Unsupported controls may still appear in
Home Assistant, but the encoder rejects them. Capability diagnostic sensors
identify optional groups; hide unsupported entities in your dashboard. Settings
show the last reported state, not optimistic confirmation. Check the BLE and
protocol status before relying on a retained setting value after disconnect.

Buttons provide Stop, lid open, seat open, close, full/small flush, rear/gentle/front
wash, dryer, nozzle rinse, rear/front nozzle cleaning and nozzle-guard cleaning.
Wash and dryer require reported occupancy. Cleaning buttons are physical actions,
not sensor queries. No actuation runs automatically at boot or on reconnect.

The six wash preferences apply to the next explicit wash button press. Editing a
preference does not start a wash. Press the wash button again to apply changes
while washing. Pressure is 1–6, position 1–5, width 1–3, and massage mode 0–5.
Massage 0 disables massage; the remaining indices follow the app's encoder.
Front wash ignores massage. Water levels 0–5 map to Off/32/34/36/38/40 °C in the
APK resources. Dryer levels are 0–2. Temperature labels are app settings, not
measured water/air temperature. One shared profile serves all wash buttons.

Configuration switches cover automatic lid, automatic deodorizer, blue LED,
operation sound, pre-wash, automatic flushing, power saving, heater automatic off
and freeze protection, subject to the table. Seat temperature offers the app's
Off/28/30/32/34/36 °C settings on supported variants.

Cycle ion mode advances Off → Bowl disinfection → Room refresh → Off. Its reported
codes are 0/1/2. Cycle flush delay advances 2 → 6 → 10 → 15 seconds → 2 seconds.
After a setting command, further setting commands wait for the corresponding
reported field to change or for reconnection. There are no blind toggle retries.
Other controls, including Stop, remain usable. The pending diagnostic reports
this wait. An unchanged status packet does not clear it.

Descaling buttons are disabled in Home Assistant by default and also require
'Enable maintenance commands', which resets OFF at boot. Use the toilet manual's
preparation and chemical procedure before starting. Start requires an unoccupied
seat; drainage retry additionally requires a reported drainage error. Neither is
triggered automatically. Recommended/active/error flags are reported separately.

Native toilet schedules, factory reset, foam, generic illumination and other
methods without a confirmed UI path are not exposed. Use Home Assistant
schedules around supported switches. Bluetooth Classic models need a separate
transport implementation; this package does not support them.

For reproducible deployments, replace both `@main` references, in the package
source and external component source, with a tested commit. Credentials remain
in your local YAML/secrets file. Existing API, Wi-Fi and OTA credentials must be
preserved when updating an installed device.

## Additional command validation

Host tests cover nonlinear gentle-wash opcodes, wash parameter packing, preserved
common settings, occupancy and maintenance guards, toggle no-ops, known/unknown
model and region combinations, and grade-dependent lid support. These tests do
not establish physical actuation on untested variants.

The expanded configuration compiled on ESPHome 2026.9.1 for ESP32-C3: RAM
138280 bytes, application flash 1212420 bytes. Hardware command tests pending.
