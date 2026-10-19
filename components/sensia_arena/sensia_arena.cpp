#include "sensia_arena.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::sensia_arena {
static const char* const TAG = "sensia_arena";
static const char* const SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* const TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
static const char* const RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

void SensiaArena::setup() {
  using esp32_ble::ESPBTUUID;
  using esp32_ble_server::BLECharacteristic;
  this->session_.settings = this->initial_settings_;
  this->service_ = this->server_->create_service(ESPBTUUID::from_raw(SERVICE_UUID), true, 8);
  if (this->service_ == nullptr) {
    this->mark_failed();
    return;
  }
  auto* tx = this->service_->create_characteristic(
      TX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  this->cccd_ = new SensiaCCCD();
  tx->add_descriptor(this->cccd_);
  auto* rx = this->service_->create_characteristic(
      RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->on_write([this](std::span<const uint8_t> data, uint16_t connection) { this->receive_(data, connection); });
  this->server_->set_advertising_required(true);
  this->server_->enqueue_start_service(this->service_);
  this->publish_();
}

void SensiaArena::reset_() {
  this->connected_ = false;
  this->authenticated_ = false;
  this->command_waiting_ = false;
  this->session_ = Session{};
  this->session_.settings = this->initial_settings_;
  this->protocol_status_ = "Disconnected";
  this->publish_();
}

void SensiaArena::ble_before_disabled_event_handler() { this->reset_(); }

void SensiaArena::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t interface,
                                      esp_ble_gatts_cb_param_t* param) {
  if (interface != this->server_->get_gatts_if()) return;
  if (event == ESP_GATTS_ADD_CHAR_EVT && param->add_char.status == ESP_GATT_OK &&
      esp32_ble::ESPBTUUID::from_uuid(param->add_char.char_uuid) == esp32_ble::ESPBTUUID::from_raw(TX_UUID)) {
    this->tx_handle_ = param->add_char.attr_handle;
  } else if (event == ESP_GATTS_CONNECT_EVT) {
    this->reset_();
    this->connection_ = param->connect.conn_id;
    std::memcpy(this->peer_, param->connect.remote_bda, sizeof(this->peer_));
    this->connected_ = true;
    this->protocol_status_ = "BLE connected; awaiting packets";
    this->publish_();
  } else if (event == ESP_GATTS_DISCONNECT_EVT && param->disconnect.conn_id == this->connection_) {
    this->reset_();
  } else if (event == ESP_GATTS_WRITE_EVT && this->cccd_ != nullptr && param->write.conn_id == this->connection_ &&
             param->write.handle == this->cccd_->handle() && !param->write.is_prep && param->write.len == 2) {
    this->session_.subscribed = (param->write.value[0] & 1) != 0;
  }
}

void SensiaArena::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  if (event == ESP_GAP_BLE_SEC_REQ_EVT && this->connected_ &&
      std::memcmp(this->peer_, param->ble_security.ble_req.bd_addr, sizeof(this->peer_)) == 0) {
    auto result = esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, this->replies_enabled_);
    if (result != ESP_OK) ESP_LOGW(TAG, "Security response failed: %d", result);
  } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT && this->connected_ &&
             std::memcmp(this->peer_, param->ble_security.auth_cmpl.bd_addr, sizeof(this->peer_)) == 0) {
    this->authenticated_ = param->ble_security.auth_cmpl.success;
    if (!this->authenticated_) {
      this->protocol_status_ = "Authentication failed";
      this->publish_();
    }
    ESP_LOGD(TAG, "Authentication: success=%d reason=0x%02x", this->authenticated_,
             param->ble_security.auth_cmpl.fail_reason);
  }
}

void SensiaArena::receive_(std::span<const uint8_t> data, uint16_t connection) {
  if (!this->connected_ || connection != this->connection_ || !this->authenticated_) return;
  const auto kind = this->session_.receive(data.data(), data.size());
  if (kind == Packet::INVALID) {
    ESP_LOGW(TAG, "Ignored packet of %u bytes", unsigned(data.size()));
    return;
  }
  // Pairing frames contain identity bytes. Log packet type only.
  if (kind == Packet::PAIRING) {
    this->protocol_status_ = "Pairing packet received";
  } else {
    if (this->command_waiting_ && this->session_.observed(this->pending_action_) != this->pending_before_)
      this->command_waiting_ = false;
    this->protocol_status_ = this->session_.known_model() ? "Status received" : "Unknown variant; monitoring only";
  }
  this->publish_();
}

bool SensiaArena::send_(Frame& frame) {
  if (!this->connected_ || !this->authenticated_ || !this->session_.subscribed || this->tx_handle_ == 0xFFFF)
    return false;
  // The server's notify() is void; use the IDF result to retain unsent
  // handshake replies.
  auto result = esp_ble_gatts_send_indicate(this->server_->get_gatts_if(), this->connection_, this->tx_handle_,
                                            frame.size(), frame.data(), false);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Notification failed: %d", result);
    return false;
  }
  return true;
}

void SensiaArena::loop() {
  if (!this->replies_enabled_ || !this->session_.pending_reply || !this->session_.subscribed) return;
  const uint32_t now_ms = millis();
  if (now_ms - this->last_reply_attempt_ < 250) return;
  this->last_reply_attempt_ = now_ms;
  auto now = this->clock_->now();
  if (!now.is_valid()) return;
  auto frame = this->session_.reply(now.month, now.day_of_month, now.hour, now.minute);
  if (frame && this->send_(*frame)) {
    this->session_.pending_reply = false;
    ESP_LOGD(TAG, "Pairing reply sent, phase %u", this->session_.phase);
  }
}

bool SensiaArena::send_command(Action action, int value) {
  auto now = this->clock_->now();
  if (!this->replies_enabled_ || !now.is_valid() || !this->connected_ || !this->authenticated_ ||
      !this->session_.subscribed || (this->session_.observed(action) >= 0 && this->command_waiting_)) {
    ESP_LOGW(TAG,
             "Command rejected: connection, clock, subscription, replies "
             "or pending setting");
    return false;
  }
  if (action == Action::DRY) value = this->dryer_level_;
  auto frame = this->session_.command(action, value, this->wash_, now.month, now.day_of_month, now.hour, now.minute,
                                      this->maintenance_enabled_);
  if (!frame) {
    ESP_LOGW(TAG, "Command rejected: unsupported, unchanged or precondition unmet");
    return false;
  }
  if (!this->send_(*frame)) return false;
  if (this->session_.observed(action) >= 0) {
    // Never blindly retry a toggle. A changed status or reconnect releases this
    // guard.
    this->command_waiting_ = true;
    this->pending_action_ = action;
    this->pending_before_ = this->session_.observed(action);
    this->publish_();
  }
  ESP_LOGD(TAG, "Command opcode %02X sent", (*frame)[1]);
  return true;
}

void SensiaArena::set_preference(Preference preference, int value) {
  switch (preference) {
    case Preference::PRESSURE:
      this->wash_.pressure = value - 1;
      break;
    case Preference::WATER:
      this->wash_.water = value;
      break;
    case Preference::WIDTH:
      this->wash_.width = value - 1;
      break;
    case Preference::POSITION:
      this->wash_.position = value - 1;
      break;
    case Preference::MASSAGE:
      this->wash_.massage = value;
      break;
    case Preference::DRYER:
      this->dryer_level_ = value;
      break;
  }
}

void SensiaArena::publish_() {
  for (auto* listener : this->listeners_) listener->on_sensia_state();
}

void SensiaArena::dump_config() {
  ESP_LOGCONFIG(TAG, "Sensia Arena BLE peripheral:");
  ESP_LOGCONFIG(TAG, "  Initial settings: 0x%02X", this->initial_settings_);
  ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->connected_));
}
}  // namespace esphome::sensia_arena
