#pragma once

#include "esphome/components/esp32_ble_server/ble_2902.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "protocol.h"

namespace esphome::sensia_arena {

class SensiaListener {
 public:
  virtual void on_sensia_state() = 0;
};

// Observe CCCD writes without replacing the BLE server's subscription callback.
class SensiaCCCD final : public esp32_ble_server::BLE2902 {
 public:
  uint16_t handle() const { return this->handle_; }
};

enum class Preference { PRESSURE, WATER, WIDTH, POSITION, MASSAGE, DRYER };

class SensiaArena final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH + 20; }
  void set_server(esp32_ble_server::BLEServer* server) { this->server_ = server; }
  void set_time(time::RealTimeClock* clock) { this->clock_ = clock; }
  void set_initial_settings(uint8_t value) { this->initial_settings_ = value; }
  void add_listener(SensiaListener* listener) { this->listeners_.push_back(listener); }
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t interface, esp_ble_gatts_cb_param_t* param);
  void ble_before_disabled_event_handler();
  bool send_command(Action action, int value = 0);
  void set_preference(Preference preference, int value);
  void set_pairing_replies(bool enabled) { this->replies_enabled_ = enabled; }
  void set_maintenance(bool enabled) { this->maintenance_enabled_ = enabled; }
  const Session& session() const { return this->session_; }
  bool connected() const { return this->connected_; }
  bool command_pending() const { return this->command_waiting_; }
  const char* protocol_status() const { return this->protocol_status_; }

 protected:
  void receive_(std::span<const uint8_t> data, uint16_t connection);
  void reset_();
  void publish_();
  bool send_(Frame& frame);
  Session session_{};
  WashProfile wash_{};
  int dryer_level_{1};
  uint8_t initial_settings_{0x06};
  bool connected_{false};
  bool replies_enabled_{false};
  bool maintenance_enabled_{false};
  bool authenticated_{false};
  bool command_waiting_{false};
  Action pending_action_{Action::STOP};
  int pending_before_{-1};
  uint32_t last_reply_attempt_{0};
  uint16_t connection_{0};
  uint16_t tx_handle_{0xFFFF};
  esp_bd_addr_t peer_{};
  const char* protocol_status_{"Waiting for toilet"};
  esp32_ble_server::BLEServer* server_{nullptr};
  esp32_ble_server::BLEService* service_{nullptr};
  SensiaCCCD* cccd_{nullptr};
  time::RealTimeClock* clock_{nullptr};
  std::vector<SensiaListener*> listeners_;
};

}  // namespace esphome::sensia_arena
