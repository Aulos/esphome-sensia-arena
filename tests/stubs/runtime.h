#pragma once
// Minimal test doubles for the hub's platform boundary. Not used in firmware.
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>
using esp_gatt_if_t = int;
using esp_bd_addr_t = uint8_t[6];
using esp_gatts_cb_event_t = int;
using esp_gap_ble_cb_event_t = int;
constexpr int ESP_OK = 0, ESP_GATT_OK = 0, ESP_GATTS_ADD_CHAR_EVT = 1, ESP_GATTS_CONNECT_EVT = 2,
              ESP_GATTS_DISCONNECT_EVT = 3, ESP_GATTS_WRITE_EVT = 4, ESP_GAP_BLE_SEC_REQ_EVT = 5,
              ESP_GAP_BLE_AUTH_CMPL_EVT = 6;
struct esp_ble_gatts_cb_param_t {
  struct {
    int status{};
    std::string char_uuid;
    uint16_t attr_handle{};
  } add_char;
  struct {
    uint16_t conn_id{};
    esp_bd_addr_t remote_bda{};
  } connect;
  struct {
    uint16_t conn_id{};
  } disconnect;
  struct {
    uint16_t conn_id{}, handle{}, len{};
    bool is_prep{};
    uint8_t* value{};
  } write;
};
struct esp_ble_gap_cb_param_t {
  struct {
    struct {
      esp_bd_addr_t bd_addr{};
    } ble_req;
    struct {
      esp_bd_addr_t bd_addr{};
      bool success{};
      int fail_reason{};
    } auth_cmpl;
  } ble_security;
};
namespace fake {
inline uint32_t now_ms = 1000;
inline int send_result = ESP_OK, attempts = 0, security_accept = -1;
inline std::array<uint8_t, 20> last_frame{};
}  // namespace fake
inline int esp_ble_gap_security_rsp(uint8_t*, bool accept) {
  fake::security_accept = accept;
  return ESP_OK;
}
inline int esp_ble_gatts_send_indicate(int, uint16_t, uint16_t, size_t size, uint8_t* data, bool) {
  ++fake::attempts;
  if (size == 20) std::copy(data, data + size, fake::last_frame.begin());
  return fake::send_result;
}
namespace esphome {
inline uint32_t millis() { return fake::now_ms; }
namespace setup_priority {
constexpr float AFTER_BLUETOOTH = 300;
}
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
  void mark_failed() {}
};
namespace esp32_ble {
struct ESPBTUUID {
  std::string uuid;
  static ESPBTUUID from_raw(const std::string& value) { return {value}; }
  static ESPBTUUID from_uuid(const std::string& value) { return {value}; }
  bool operator==(const ESPBTUUID& other) const { return uuid == other.uuid; }
};
}  // namespace esp32_ble
namespace esp32_ble_server {
class BLE2902 {
 public:
  virtual ~BLE2902() = default;

 protected:
  uint16_t handle_{42};
};
class BLECharacteristic {
 public:
  static constexpr int PROPERTY_WRITE = 1, PROPERTY_NOTIFY = 2, PROPERTY_WRITE_NR = 4;
  std::function<void(std::span<const uint8_t>, uint16_t)> write;
  void add_descriptor(BLE2902* value) { descriptor = value; }
  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> value) { write = value; }
  ~BLECharacteristic() { delete descriptor; }
  BLE2902* descriptor{};
};
class BLEService {
 public:
  BLECharacteristic tx, rx;
  int created{};
  BLECharacteristic* create_characteristic(const char*, int) { return created++ == 0 ? &tx : &rx; }
};
class BLEServer {
 public:
  BLEService service;
  BLEService* create_service(esp32_ble::ESPBTUUID, bool, int) { return &service; }
  void set_advertising_required(bool) {}
  void enqueue_start_service(BLEService*) {}
  int get_gatts_if() const { return 7; }
};
}  // namespace esp32_ble_server
namespace time {
struct ESPTime {
  bool valid{true};
  unsigned month{10}, day_of_month{19}, hour{12}, minute{30};
  bool is_valid() const { return valid; }
};
class RealTimeClock {
 public:
  ESPTime value;
  ESPTime now() const { return value; }
};
}  // namespace time
template <typename... Args>
void test_log(Args...) {}
}  // namespace esphome
#define ESP_LOGW(...) esphome::test_log(__VA_ARGS__)
#define ESP_LOGD(...) esphome::test_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) esphome::test_log(__VA_ARGS__)
#define YESNO(x) ((x) ? "YES" : "NO")
