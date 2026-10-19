# Sensia Arena upstream ESPHome research

Researched 2026-10-19. Local implementation inspected at commit
`b2dca895d3bd90db096e7459aea2f40532eea42c`. This is a submission plan, not
an upstream acceptance commitment. No firmware was changed during this research.

## Assessment

Upstreaming is technically plausible. The working protocol and hardware evidence
are useful foundations, but the current YAML package is not ready to submit as
a native ESPHome integration. Most work is component engineering, regression
coverage, documentation, and review, rather than further protocol discovery for
the already verified functions.

## Official submission requirements

ESPHome requires passing CI and a completed PR template. Its current guidance
requires the first new-component PR to contain the hub and one entity platform;
additional platforms belong in separate PRs. Changes exceeding 1,000 non-test
lines or 15 components/areas receive a `too-big` label and are not reviewed in
the normal process. Discuss large submissions with maintainers first. AI-assisted
code is allowed, but the submitting author must understand and defend it.
Review time is not guaranteed. [Submission guide](https://developers.esphome.io/contributing/submitting-your-work/)

A native component needs configuration validation and Python code generation,
registration with the component lifecycle, appropriate dependencies, and its
own namespace. Current metadata includes a required `DOMAIN`; `CODEOWNERS`
identifies the responsible GitHub maintainer. Entity platforms should use
ESPHome's entity classes and schema helpers.
[Component architecture](https://developers.esphome.io/architecture/components/)

Use current upstream coding conventions and avoid repeated runtime heap
allocation. Fixed-size protocol packets are a good candidate for fixed buffers.
[Code contribution guide](https://developers.esphome.io/contributing/code/)

The code PR needs a linked documentation PR for the new public functionality.
Prepare a minimal YAML example and component documentation in `esphome/esphome.io`.
[PR template](https://github.com/esphome/esphome/blob/dev/.github/PULL_REQUEST_TEMPLATE.md)
[Documentation guide](https://developers.esphome.io/contributing/docs/)

ESPHome licenses runtime/C++ under GPLv3 and Python under MIT. Ensure the submitted
implementation can use those terms, and keep APK/decompiled source out of the
submission. No ESPHome-specific CLA requirement was found in the reviewed guides;
do not assume Home Assistant Core's contribution rules apply.
[ESPHome license](https://github.com/esphome/esphome/blob/dev/LICENSE)

## Concrete gaps in this repository

These are findings from the local source, with proposed remedies. They are not
claims that maintainers have already requested these changes.

| Current evidence | Work needed |
|---|---|
| `components/sensia_arena/__init__.py` has only an empty schema and an injected include | Add a native hub, typed schema, IDs, dependencies, metadata, and code generation |
| `packages/sensia-arena.yaml` contains 682 lines of BLE setup, lambdas, scripts and template entities | Move protocol transport and state management into C++; expose native entity platforms; keep the example YAML small |
| `protocol.h` defines a global `inline Session session` | Give the hub ownership of its session; explicitly constrain initial implementation to one toilet per ESP32 |
| YAML hard-codes board, timezone and Home Assistant time | Keep board/network choices in examples; accept a standard time component reference |
| A build flag manually reserves one GAP callback slot | Register through `esp32_ble.register_gap_event_handler()` so code generation allocates callback capacity correctly |
| Fixed 20-byte commands use newly allocated vectors | Use fixed storage internally, adapting only where the BLE API requires it |
| Most states are polled through templates | Publish on received status; define consistent unknown/disconnected behavior |
| All controls are declared, including unsupported ones | Make entity configuration optional and keep runtime capability guards; distinguish protocol flush support from an installed actuator |
| Raw RX frames are logged at DEBUG | Redact pairing identity and avoid publishing security material in logs or test fixtures |
| No explicit LICENSE or CI configuration in this repository | Resolve contribution licensing/provenance and add automated validation before submission |

The callback helper both registers the handler and reserves its slot in current
upstream source. Preserve the working BLE **peripheral/server** role; the toilet
is the central. Do not replace this with a conventional BLE client integration.
Continue to let the BLE server manage CCCD subscriptions.
[BLE code generation](https://github.com/esphome/esphome/blob/dev/esphome/components/esp32_ble/__init__.py)
[BLE server](https://github.com/esphome/esphome/blob/dev/esphome/components/esp32_ble_server/__init__.py)

## Behavior that deserves review before submission

- Pairing opcode 00 includes settings. The present startup value can overwrite
  a toilet preference before receiving its first status. Expose understandable
  startup options, preserve observed settings where possible, and document the
  unavoidable first-handshake behavior. Even a monitoring-only first PR is not
  strictly read-only at the protocol level.
- The reply loop clears pending state immediately after calling notify. Test
  subscription timing, notification failure and reconnect behavior explicitly.
- A pending setting currently blocks later settings until a changed status or
  reconnection. Define recovery from lost acknowledgements without blindly
  repeating toggle commands. Stop must remain available.
- Pairing permission currently also gates application replies. Review whether
  admission of new peers and operation of an existing bond should be separate.
- Keep occupancy guards, unknown-variant rejection and explicit maintenance
  enablement. Do not automatically retry physical actions.

These observations come from `packages/sensia-arena.yaml` and
`components/sensia_arena/protocol.h`, rather than upstream policy.

## Hardware evidence and remaining scope

Only 39354SH0, reported model 2 / grade 2 / region 2, on ESP32-C3 has physical
validation. The owner confirmed occupancy transitions, lid opening, seat opening,
rear/gentle/front wash, dryer, Stop, and all four nozzle-cleaning controls.
Front wash and cleaning confirmations are in this conversation and postdate the
current README. Update that validation record when preparing the submission.

Descaling remains unverified. Physical flush cannot be tested on this installation
because it has no electronic actuator. Other grades, regions and floor-standing
models have protocol-analysis/test evidence only. Document that distinction;
additional testers would strengthen those claims but are not presented here as
a mandatory upstream tester count.

## Suggested submission sequence

1. Refactor into a native external component and validate it on the existing
   toilet while keeping the current deployment available for rollback.
2. Discuss the proposed domain `sensia_arena`, server-role transport and staged
   scope with maintainers. This research has not contacted them.
3. First code PR: hub, pairing/status transport and `binary_sensor` platform for
   connection and occupancy, with tests and corresponding user documentation.
4. Separate follow-ups for `button`, `number`, `switch`, `select`, `sensor`, and
   `text_sensor` support. Split further if a PR exceeds the size limit. Keep
   unvalidated maintenance actions out of the first submission.
5. Test upgrade/reconnect behavior and provide a migration example that preserves
   existing entity names/IDs where possible. The grouped Home Assistant display
   names are local overrides, not yet defaults in the package.

The order is a recommendation; exact platform configuration is subject to review.
This would use Home Assistant's existing ESPHome integration and does not require
a separate Home Assistant custom integration.

## Test plan

Retain the existing host protocol vectors, then add coverage for malformed
frames, missing clock, subscription ordering, bond restoration, disconnect during
commands, status reconciliation and unknown variants. Add schema tests for invalid
ranges, missing dependencies and conflicting BLE configuration. Compile minimal
and full configurations on supported targets, and run the refactored firmware on
the C3 before claiming compatibility. Do not infer C6 hardware validation from a
successful compile. Upstream test configuration locations and commands should
follow the current development checkout rather than the deployed beta.

Upstream requires component compile configurations under
`tests/components/sensia_arena/` covering the supported architectures/frameworks
and automations. Run `script/test_build_components -e compile -c sensia_arena`
and the `-e config` variant in that checkout. Compile tests establish build
compatibility, not hardware functionality.
[Component test guide](https://developers.esphome.io/architecture/ci/component_tests/)

No runtime test was needed for this research-only change; existing test coverage
was inspected, not rerun.
