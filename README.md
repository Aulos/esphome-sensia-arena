# ESPHome Sensia Arena

Experimental pairing and status prototype for GROHE Sensia Arena 39354SH0
and Seeed Studio XIAO ESP32-C3.

This implements the BLE peripheral role recovered from Android app 1.4.1.
The toilet connects to the ESP32, which advertises as LIXIL. No phone app is
needed. The board requires USB power and Wi-Fi; no wiring to the toilet.
Attach the XIAO's supplied antenna before testing.

Hardware compatibility with this specific toilet revision is NOT verified.
The APK also contains a Classic Bluetooth path. The C3 supports BLE only;
if this revision needs Classic SPP, this firmware cannot provide it.
No automatic product/model assumption is made from the reported model code.

## Files
- `sensia-arena.yaml`: ESPHome configuration, tested with ESPHome 2026.9.1.
- `sensia_protocol.h`: Packet parser and opcode-00 response builder.
- `secrets.example.yaml`: Placeholder credentials; copy locally to `secrets.yaml`.
- `test_protocol.cpp`: Host checks with synthetic packets, not captured traffic.

## Install
1. Copy sensia-arena.yaml and sensia_protocol.h into your ESPHome configuration
   directory. Copy secrets.example.yaml to secrets.yaml, or merge its keys into
   your existing secrets.yaml. Set your 2.4 GHz Wi-Fi name/password. Generate
   the API key with `openssl rand -base64 32` and the OTA password with
   `openssl rand -hex 24`. Replace both placeholders. Never commit secrets.yaml.
2. Use ESPHome 2026.9.1 or a newer version that supports custom BLE server
   characteristics. Set the timezone substitution if outside Europe/Warsaw.
3. Review initial_settings below before enabling pairing replies.
4. Compile and install over USB with ESPHome Device Builder or:
     esphome run sensia-arena.yaml
   No firmware has been flashed by this investigation. The local build uses
   placeholder Wi-Fi, so compile again after configuring your network.
5. Add the device through Home Assistant's ESPHome integration. Use the API key
   from secrets.yaml if prompted. Inspect the ESPHome logs alongside HA entities.

## First connection
The firmware advertises automatically. Use the toilet's remote-control
smartphone-pairing procedure from the GROHE manual for 39354SH0. The ESP32 takes
the phone's role. Physical pairing and any toilet confirmation remain untested.
Manufacturer manual:
https://cdn.cloud.grohe.com/Literature/Brochures/EN/INT00009/original/INT00009.pdf

Initially leave 'Enable pairing replies' OFF. Look for a BLE connection and
'RX' entries in the ESPHome logs. This mode receives packets but sends no GROHE
application response; it may therefore not complete pairing or yield status.
Once you have reviewed initial_settings, turn the switch ON and repeat the
toilet's pairing procedure if its first attempt expired.

Replies require a valid SNTP clock, a received 20-byte pairing packet, and the
toilet subscribing to notifications. The pending response is sent after those
conditions are met. The 250ms check is a local scheduling choice, not a recovered
heartbeat interval. Failed notifications are logged by ESPHome; no retry protocol
is invented here. Reconnect/re-pair if the exchange stalls and retain the logs.

Expected progression:

```text
Waiting for toilet
BLE connected; awaiting packets
Pairing packet received
Status received
```

'Toilet BLE connection' only reports a radio connection, not authenticated pairing.
'Seat occupied' stays unknown until status arrives and becomes unknown on
disconnect or after 60 seconds without status. That freshness threshold is
provisional. A fresh status restores the value. 'Reported model code' is the
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
That is why replies require explicit opt-in and the switch resets OFF at boot.

To match known settings, initial_settings is an encoded byte:
  bits 7..6: auto-wash delay code
  bit 5: seat-heater-off flag
  bits 4..2: seat-temperature code
  bit 1: automatic washing/flushing flag
  bit 0: supplied by firmware
Do not assume the codes are temperatures in degrees. Once a status arrives,
the firmware derives these fields and drainage mode from the received status
instead of continuing to use startup defaults. No schedule configuration is sent.

For an unattended installation, enablement across reboots can be changed later
after the handshake and initial settings have been validated on the actual toilet.

## Scope and diagnostics
There are no flush, spray, dryer, reset or descaling buttons in this first build.
Only opcode 00 replies are implemented. Radio bonding/admission, firmware-specific
timing and settings effects have not been measured. A successful compile does not
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
https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

## Validation (2026-10-16)
ESPHome 2026.9.1 full ESP-IDF build succeeded for seeed_xiao_esp32c3.
Reported static RAM: 135856 bytes (42.3%); application flash: 1197068 bytes (65.2%).
Host protocol checks passed with address and undefined-behavior sanitizers.
These checks used synthetic packets. No hardware was flashed or tested.
