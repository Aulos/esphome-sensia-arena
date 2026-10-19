#include "entities.h"

#include <cmath>

namespace esphome::sensia_arena {
#ifdef USE_BINARY_SENSOR
void SensiaBinarySensor::on_sensia_state() {
  const auto& s = this->parent_->session();
  if (this->kind_ == BinaryKind::CONNECTION) {
    this->publish_state(this->parent_->connected());
    return;
  }
  if (this->kind_ == BinaryKind::PENDING) {
    this->publish_state(this->parent_->command_pending());
    return;
  }
  if (this->kind_ == BinaryKind::OCCUPIED) {
    auto occupied = this->parent_->occupancy_state();
    if (occupied.has_value())
      this->publish_state(*occupied);
    else
      this->invalidate_state();
    return;
  }
  if (!s.have_status) {
    this->invalidate_state();
    return;
  }
  switch (this->kind_) {
    case BinaryKind::MOTORIZED_LID_SUPPORTED:
      this->publish_state(s.supports(Action::OPEN_LID));
      break;
    case BinaryKind::SEAT_HEATING_SUPPORTED:
      this->publish_state(s.supports(Action::SEAT_TEMP));
      break;
    case BinaryKind::DESCALING_SUPPORTED:
      this->publish_state(s.supports(Action::DESCALE));
      break;
    case BinaryKind::ION_SUPPORTED:
      this->publish_state(s.supports(Action::ION_CYCLE));
      break;
    default:
      if (!s.supports(Action::DESCALE)) {
        this->invalidate_state();
        return;
      }
      if (this->kind_ == BinaryKind::DESCALING_RECOMMENDED) this->publish_state(s.status[5] & 1);
      if (this->kind_ == BinaryKind::DESCALING_ACTIVE) this->publish_state(s.status[5] & 2);
      if (this->kind_ == BinaryKind::DESCALING_ERROR) this->publish_state(s.status[5] & 4);
      break;
  }
}
#endif
#ifdef USE_SENSOR
void SensiaSensor::on_sensia_state() {
  const auto& s = this->parent_->session();
  if (!s.have_status) {
    this->publish_state(NAN);
    return;
  }
  switch (this->kind_) {
    case SensorKind::MODEL:
      this->publish_state(s.model);
      break;
    case SensorKind::GRADE:
      this->publish_state(s.grade);
      break;
    case SensorKind::REGION:
      this->publish_state(s.region);
      break;
    case SensorKind::FLUSH_DELAY: {
      constexpr int delays[] = {2, 6, 10, 15};
      this->publish_state(delays[s.observed(Action::DELAY_CYCLE)]);
      break;
    }
    case SensorKind::ION_MODE:
      this->publish_state(s.observed(Action::ION_CYCLE));
      break;
    case SensorKind::SEAT_TEMPERATURE_CODE:
      this->publish_state(s.observed(Action::SEAT_TEMP));
      break;
    case SensorKind::ABNORMAL_STATUS:
      this->publish_state(s.status[14] & 15);
      break;
  }
}
#endif
#ifdef USE_SWITCH
Action SensiaSwitch::action_() const {
  switch (this->kind_) {
    case SwitchKind::AUTO_LID:
      return Action::AUTO_LID;
    case SwitchKind::DEODORIZER:
      return Action::DEODORIZER;
    case SwitchKind::BLUE_LED:
      return Action::BLUE_LED;
    case SwitchKind::SOUND:
      return Action::SOUND;
    case SwitchKind::PREWASH:
      return Action::PREWASH;
    case SwitchKind::AUTO_FLUSH:
      return Action::AUTO_FLUSH;
    case SwitchKind::POWER_SAVE:
      return Action::POWER_SAVE;
    case SwitchKind::HEATER_OFF:
      return Action::HEATER_OFF;
    case SwitchKind::FREEZE:
      return Action::FREEZE;
    default:
      return Action::STOP;
  }
}
void SensiaSwitch::setup() {
  if (this->kind_ == SwitchKind::PAIRING_REPLIES)
    this->write_state(this->get_initial_state_with_restore_mode().value_or(false));
  else if (this->kind_ == SwitchKind::MAINTENANCE)
    this->write_state(false);
}
void SensiaSwitch::write_state(bool state) {
  if (this->kind_ == SwitchKind::PAIRING_REPLIES) {
    this->parent_->set_pairing_replies(state);
    this->publish_state(state);
  } else if (this->kind_ == SwitchKind::MAINTENANCE) {
    this->parent_->set_maintenance(state);
    this->publish_state(state);
  } else {
    this->parent_->send_command(this->action_(), state);
  }
}
void SensiaSwitch::on_sensia_state() {
  if (this->kind_ == SwitchKind::PAIRING_REPLIES || this->kind_ == SwitchKind::MAINTENANCE) return;
  const auto& s = this->parent_->session();
  if (s.supports(this->action_()))
    this->publish_state(s.setting(this->action_()));
  else
    this->set_has_state(false);
}
#endif
#ifdef USE_NUMBER
void SensiaNumber::setup() {
  float value = this->initial_value_;
  // Same entity preference key/type as the previous template number.
  this->preference_ = this->make_entity_preference<float>();
  this->preference_.load(&value);
  if (!std::isfinite(value) || value < this->traits.get_min_value() || value > this->traits.get_max_value() ||
      std::floor(value) != value)
    value = this->initial_value_;
  this->parent_->set_preference(this->kind_, int(value));
  this->publish_state(value);
}
void SensiaNumber::control(float value) {
  if (!std::isfinite(value) || value < this->traits.get_min_value() || value > this->traits.get_max_value() ||
      std::floor(value) != value)
    return;
  this->parent_->set_preference(this->kind_, int(value));
  this->publish_state(value);
  this->preference_.save(&value);
}
#endif
#ifdef USE_SELECT
void SensiaSelect::on_sensia_state() {
  const auto& s = this->parent_->session();
  if (!s.supports(Action::SEAT_TEMP)) {
    this->set_has_state(false);
    return;
  }
  constexpr uint8_t codes[] = {0, 1, 3, 5, 6, 7};
  for (size_t i = 0; i < 6; i++) {
    if (codes[i] == s.observed(Action::SEAT_TEMP)) {
      this->publish_state(i);
      return;
    }
  }
  this->set_has_state(false);
}
#endif
}  // namespace esphome::sensia_arena
