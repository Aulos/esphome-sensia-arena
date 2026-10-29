#include <cassert>
#include <iostream>

#include "../components/sensia_arena/sensia_arena.h"
using namespace esphome;
using namespace esphome::sensia_arena;
int main() {
  esp32_ble_server::BLEServer server;
  time::RealTimeClock clock;
  SensiaArena hub;
  hub.set_server(&server);
  hub.set_time(&clock);
  hub.setup();
  assert(!hub.occupancy_state().has_value());
  esp_ble_gatts_cb_param_t event{};
  event.add_char.char_uuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  event.add_char.attr_handle = 41;
  hub.gatts_event_handler(ESP_GATTS_ADD_CHAR_EVT, 7, &event);
  event.connect.conn_id = 1;
  hub.gatts_event_handler(ESP_GATTS_CONNECT_EVT, 7, &event);
  assert(hub.connected());
  esp_ble_gap_cb_param_t security{};
  hub.gap_event_handler(ESP_GAP_BLE_SEC_REQ_EVT, &security);
  assert(fake::security_accept == 0);
  hub.set_pairing_replies(true);
  hub.gap_event_handler(ESP_GAP_BLE_SEC_REQ_EVT, &security);
  assert(fake::security_accept == 1);
  std::array<uint8_t, 20> pairing{};
  pairing[9] = 1;
  server.service.rx.write(pairing, 1);
  assert(!hub.session().have_identity);  // Ignore unauthenticated writes.
  security.ble_security.auth_cmpl.success = true;
  hub.gap_event_handler(ESP_GAP_BLE_AUTH_CMPL_EVT, &security);
  server.service.rx.write(pairing, 2);
  assert(!hub.session().have_identity);  // Wrong connection.
  server.service.rx.write(pairing, 1);
  assert(hub.session().pending_reply);
  hub.loop();
  assert(fake::attempts == 0);  // No subscription yet.
  uint8_t cccd[] = {1, 0};
  event.write = {1, 42, 2, false, cccd};
  hub.gatts_event_handler(ESP_GATTS_WRITE_EVT, 7, &event);
  clock.value.valid = false;
  fake::now_ms += 250;
  hub.loop();
  assert(fake::attempts == 0);
  clock.value.valid = true;
  fake::send_result = -1;
  fake::now_ms += 250;
  hub.loop();
  assert(fake::attempts == 1 && hub.session().pending_reply);
  fake::send_result = ESP_OK;
  fake::now_ms += 250;
  hub.loop();
  assert(fake::attempts == 2 && !hub.session().pending_reply);
  const std::array<uint8_t, 20> status = {0,    0,    0, 0,    0, 1,    0, 0,    0x51, 0x91,
                                          0x0E, 0x12, 0, 0x29, 0, 0xC1, 0, 0x12, 0x90, 0xFE};
  server.service.rx.write(status, 1);
  fake::now_ms += 250;
  hub.loop();
  assert(hub.session().have_status);
  assert(hub.send_command(Action::TEST_COMMAND_1) && fake::last_frame[1] == 0x17);
  assert(hub.send_command(Action::TEST_COMMAND_2) && fake::last_frame[1] == 0x57);
  assert(!hub.send_command(Action::REAR));     // Occupancy interlock.
  assert(!hub.send_command(Action::DESCALE));  // Maintenance interlock.
  fake::send_result = -1;
  assert(!hub.send_command(Action::BLUE_LED, 0) && !hub.command_pending());
  fake::send_result = ESP_OK;
  assert(hub.send_command(Action::BLUE_LED, 0) && hub.command_pending());
  int count = fake::attempts;
  assert(!hub.send_command(Action::BLUE_LED, 0) && fake::attempts == count);
  assert(hub.send_command(Action::STOP));  // Stop is not blocked by a pending setting.
  server.service.rx.write(status, 1);
  assert(hub.command_pending());
  auto changed = status;
  changed[10] &= ~2;
  server.service.rx.write(changed, 1);
  assert(!hub.command_pending());
  changed[10] |= 0x20;
  server.service.rx.write(changed, 1);
  hub.set_preference(Preference::PRESSURE, 6);
  assert(hub.send_command(Action::REAR) && fake::last_frame[1] == 0x7E);
  assert(hub.send_command(Action::DRY) && fake::last_frame[1] == 0x6B);
  // Brief reconnects keep the display stable, but never retain command authority.
  assert(hub.occupancy_state() == true);
  event.disconnect.conn_id = 1;
  const auto lost_at = fake::now_ms;
  hub.gatts_event_handler(ESP_GATTS_DISCONNECT_EVT, 7, &event);
  assert(!hub.connected() && !hub.session().have_status && !hub.session().subscribed);
  assert(hub.occupancy_state() == true);
  assert(!hub.send_command(Action::REAR));
  assert(!hub.send_command(Action::STOP));
  fake::now_ms = lost_at + 5000;
  hub.gatts_event_handler(ESP_GATTS_CONNECT_EVT, 7, &event);
  hub.gap_event_handler(ESP_GAP_BLE_AUTH_CMPL_EVT, &security);
  event.write = {1, 42, 2, false, cccd};
  hub.gatts_event_handler(ESP_GATTS_WRITE_EVT, 7, &event);
  server.service.rx.write(pairing, 1);
  assert(hub.occupancy_state() == true && !hub.session().have_status);
  assert(!hub.send_command(Action::REAR));  // Cached occupancy cannot authorize a wash.
  server.service.rx.write(status, 1);
  assert(hub.occupancy_state() == false);  // Fresh status wins immediately.
  fake::now_ms = lost_at + 16000;
  hub.loop();
  assert(hub.occupancy_state() == false);  // Old expiry cannot invalidate new status.

  event.disconnect.conn_id = 1;
  const auto second_loss = fake::now_ms;
  hub.gatts_event_handler(ESP_GATTS_DISCONNECT_EVT, 7, &event);
  fake::now_ms = second_loss + 14000;
  hub.gatts_event_handler(ESP_GATTS_CONNECT_EVT, 7, &event);
  hub.loop();
  assert(hub.occupancy_state() == false);
  fake::now_ms = second_loss + 15000;
  hub.loop();
  assert(!hub.occupancy_state().has_value());  // Reconnect alone cannot extend grace.

  hub.gap_event_handler(ESP_GAP_BLE_AUTH_CMPL_EVT, &security);
  server.service.rx.write(changed, 1);
  assert(hub.occupancy_state() == true);
  fake::now_ms = UINT32_MAX - 5000;
  hub.gatts_event_handler(ESP_GATTS_DISCONNECT_EVT, 7, &event);
  fake::now_ms += 14999;
  hub.loop();
  assert(hub.occupancy_state() == true);
  ++fake::now_ms;
  hub.loop();
  assert(!hub.occupancy_state().has_value());  // Timer wrap is safe.
  hub.gatts_event_handler(ESP_GATTS_CONNECT_EVT, 7, &event);
  hub.gap_event_handler(ESP_GAP_BLE_AUTH_CMPL_EVT, &security);
  server.service.rx.write(changed, 1);
  assert(hub.occupancy_state() == true);
  hub.ble_before_disabled_event_handler();
  assert(!hub.connected() && !hub.occupancy_state().has_value());
  std::cout << "Native hub lifecycle checks passed\n";
}
