#pragma once
#include "esphome/core/preferences.h"
#include "sensia_arena.h"
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::sensia_arena {
enum class BinaryKind {
  CONNECTION,
  OCCUPIED,
  DESCALING_RECOMMENDED,
  DESCALING_ACTIVE,
  DESCALING_ERROR,
  PENDING,
  MOTORIZED_LID_SUPPORTED,
  SEAT_HEATING_SUPPORTED,
  DESCALING_SUPPORTED,
  ION_SUPPORTED
};
enum class SensorKind { MODEL, GRADE, REGION, FLUSH_DELAY, ION_MODE, SEAT_TEMPERATURE_CODE, ABNORMAL_STATUS };
enum class SwitchKind {
  PAIRING_REPLIES,
  MAINTENANCE,
  AUTO_LID,
  DEODORIZER,
  BLUE_LED,
  SOUND,
  PREWASH,
  AUTO_FLUSH,
  POWER_SAVE,
  HEATER_OFF,
  FREEZE
};

#ifdef USE_BUTTON
class SensiaButton final : public button::Button, public Parented<SensiaArena> {
 public:
  void set_kind(Action kind) { this->kind_ = kind; }

 protected:
  void press_action() override { this->parent_->send_command(this->kind_); }
  Action kind_;
};
#endif
#ifdef USE_BINARY_SENSOR
class SensiaBinarySensor final : public binary_sensor::BinarySensor,
                                 public Parented<SensiaArena>,
                                 public SensiaListener {
 public:
  void set_kind(BinaryKind kind) { this->kind_ = kind; }
  void on_sensia_state() override;

 protected:
  BinaryKind kind_;
};
#endif
#ifdef USE_SENSOR
class SensiaSensor final : public sensor::Sensor, public Parented<SensiaArena>, public SensiaListener {
 public:
  void set_kind(SensorKind kind) { this->kind_ = kind; }
  void on_sensia_state() override;

 protected:
  SensorKind kind_;
};
#endif
#ifdef USE_TEXT_SENSOR
class SensiaTextSensor final : public text_sensor::TextSensor, public Parented<SensiaArena>, public SensiaListener {
 public:
  void on_sensia_state() override { this->publish_state(this->parent_->protocol_status()); }
};
#endif
#ifdef USE_SWITCH
class SensiaSwitch final : public switch_::Switch,
                           public Component,
                           public Parented<SensiaArena>,
                           public SensiaListener {
 public:
  void set_kind(SwitchKind kind) { this->kind_ = kind; }
  void setup() override;
  void on_sensia_state() override;

 protected:
  void write_state(bool state) override;
  Action action_() const;
  SwitchKind kind_;
};
#endif
#ifdef USE_NUMBER
class SensiaNumber final : public number::Number, public Component, public Parented<SensiaArena> {
 public:
  void set_kind(Preference kind) { this->kind_ = kind; }
  void set_initial_value(float value) { this->initial_value_ = value; }
  void setup() override;

 protected:
  void control(float value) override;
  Preference kind_;
  float initial_value_{0};
  ESPPreferenceObject preference_;
};
#endif
#ifdef USE_SELECT
class SensiaSelect final : public select::Select, public Parented<SensiaArena>, public SensiaListener {
 public:
  void on_sensia_state() override;

 protected:
  void control(size_t index) override { this->parent_->send_command(Action::SEAT_TEMP, index); }
};
#endif
}  // namespace esphome::sensia_arena
