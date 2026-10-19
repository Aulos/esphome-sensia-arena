#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Derived from APK 1.4.1 BLEManager and NToiletInfoManager; pairing/status verified on 39354SH0 (2026-10-19).
namespace sensia {
enum class Packet { INVALID, PAIRING, STATUS };
enum class Action {
  STOP, OPEN_LID, OPEN_SEAT, CLOSE, FLUSH_FULL, FLUSH_SMALL,
  REAR, GENTLE, FRONT, DRY, CLEAN_NOZZLE, CLEAN_REAR, CLEAN_FRONT, CLEAN_GUARD,
  AUTO_LID, DEODORIZER, BLUE_LED, SOUND, PREWASH, AUTO_FLUSH,
  ION_CYCLE, DELAY_CYCLE, POWER_SAVE, HEATER_OFF, FREEZE, SEAT_TEMP,
  DESCALE, RETRY_DRAIN
};
struct WashProfile {
  int pressure{2}, water{1}, width{0}, position{2}, massage{0};
  bool valid() const {
    return pressure >= 0 && pressure <= 5 && water >= 0 && water <= 5 &&
           width >= 0 && width <= 2 && position >= 0 && position <= 4 &&
           massage >= 0 && massage <= 5;
  }
};

struct Session {
  std::array<uint8_t, 6> identity{};
  bool have_identity{false};
  bool have_status{false};
  bool pending_reply{false};
  bool subscribed{false};
  uint8_t phase{1};
  uint8_t settings{0x06};
  uint8_t drainage{0};
  bool occupied{false};
  uint8_t model{0};
  uint8_t grade{0};
  uint8_t region{0};
  std::array<uint8_t, 20> status{};

  bool known_model() const { return (model == 2 || model == 3) && region >= 1 && region <= 4 && grade >= 1 && grade <= 3; }
  bool heated_seat() const { return model == 2 && (region == 3 || region == 4); }
  bool supports(Action a) const {
    if (!have_status || !known_model() || int(a) < int(Action::STOP) ||
        int(a) > int(Action::RETRY_DRAIN)) return false;
    switch (a) {
      case Action::OPEN_LID: case Action::OPEN_SEAT: case Action::CLOSE:
      case Action::AUTO_LID: return grade == 2 || grade == 3;
      case Action::BLUE_LED: case Action::PREWASH: case Action::ION_CYCLE:
        return model == 2 && region >= 2 && region <= 4;
      case Action::POWER_SAVE: case Action::HEATER_OFF: case Action::SEAT_TEMP:
        return heated_seat();
      case Action::FREEZE: return model == 2 && region == 3;
      case Action::DESCALE: case Action::RETRY_DRAIN:
        return model == 2 && (region == 2 || region == 4);
      default: return true;
    }
  }
  int setting(Action a) const {
    switch (a) {
      case Action::AUTO_LID: return !(status[10] & 8);
      case Action::DEODORIZER: return bool(status[10] & 4);
      case Action::BLUE_LED: return bool(status[10] & 2);
      case Action::SOUND: return bool(status[12] & 8);
      case Action::PREWASH: return bool(status[17] & 8);
      case Action::AUTO_FLUSH: return bool(status[13] & 8);
      case Action::POWER_SAVE: return bool(status[10] & 1);
      case Action::HEATER_OFF: return bool(status[12] & 4);
      case Action::FREEZE: return bool(status[12] & 16);
      default: return -1;
    }
  }

  int observed(Action a) const {
    if (a == Action::ION_CYCLE) return (status[17] >> 4) & 3;
    if (a == Action::DELAY_CYCLE) return (status[11] >> 5) & 3;
    if (a == Action::SEAT_TEMP) return status[13] >> 5;
    return setting(a);
  }

  Packet receive(const uint8_t *p, size_t size) {
    if (size != 20) return Packet::INVALID;
    if ((p[19] & 0xF0) == 0) {
      for (size_t i = 0; i < identity.size(); ++i) identity[i] = p[i + 3];
      have_identity = true;
      phase = p[9] == 1 ? 1 : 2;
      pending_reply = true;
      return Packet::PAIRING;
    }
    for (size_t i = 0; i < status.size(); ++i) status[i] = p[i];
    region = p[17] & 7;
    // Preserve the received settings before acknowledging the first status.
    settings = uint8_t(((p[11] & 0x60) << 1) | ((p[12] & 0x04) << 3) |
                       ((p[13] & 0xE0) >> 3) | ((p[13] & 0x08) >> 2));
    drainage = (p[12] & 0x20) ? 0x80 : 0;
    occupied = (p[10] & 0x20) != 0;
    model = (p[9] & 0xC0) >> 6;
    grade = (p[9] & 0x38) >> 3;
    if (!have_status && have_identity && phase == 1) pending_reply = true;
    have_status = true;
    phase = 2;
    return Packet::STATUS;
  }

  std::vector<uint8_t> reply(unsigned month, unsigned day, unsigned hour,
                             unsigned minute) const {
    if (!have_identity || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59) return {};
    std::vector<uint8_t> p(20, 0);
    p[3] = drainage;
    p[4] = settings | 1;
    p[5] = uint8_t(((month - 1) << 4) | ((day - 1) >> 1));
    p[6] = uint8_t(((day - 1) << 7) | (hour << 2) | (minute >> 4));
    p[7] = uint8_t(minute << 4);
    p[10] = 0x40;
    for (size_t i = 0; i < identity.size(); ++i) p[i + 13] = identity[i];
    p[19] = phase;
    return p;
  }
  std::vector<uint8_t> command(Action a, int value, const WashProfile &wash,
                              unsigned month, unsigned day, unsigned hour,
                              unsigned minute, bool maintenance = false) const {
    if (!have_identity || phase != 2 || !supports(a)) return {};
    const bool washing = a == Action::REAR || a == Action::GENTLE || a == Action::FRONT;
    if ((washing || a == Action::DRY) && !occupied) return {};
    if ((status[5] & 2) && a != Action::STOP && a != Action::RETRY_DRAIN) return {};
    if (a == Action::DESCALE || a == Action::RETRY_DRAIN) {
      if (!maintenance || occupied) return {};
      if (a == Action::DESCALE && (status[5] & 2)) return {};
      if (a == Action::RETRY_DRAIN && !(status[5] & 4)) return {};
    }
    int current = setting(a);
    if (current >= 0 && (value < 0 || value > 1 || current == value)) return {};
    auto p = reply(month, day, hour, minute);
    if (p.empty()) return p;
    switch (a) {
      case Action::STOP: p[1] = 0xF2; break;
      case Action::OPEN_LID: p[1] = 0x37; break;
      case Action::OPEN_SEAT: p[1] = 0x27; break;
      case Action::CLOSE: p[1] = 0x47; break;
      case Action::FLUSH_FULL: p[1] = 0xF3; break;
      case Action::FLUSH_SMALL: p[1] = 0xB3; break;
      case Action::CLEAN_NOZZLE: p[1] = 0x31; break;
      case Action::CLEAN_REAR: p[1] = 0x4A; break;
      case Action::CLEAN_FRONT: p[1] = 0xCA; break;
      case Action::CLEAN_GUARD: p[1] = 0xDA; break;
      case Action::AUTO_LID: p[1] = value ? 0xA7 : 0x87; break;
      case Action::DEODORIZER: p[1] = 0xD9; break;
      case Action::BLUE_LED: p[1] = 0x43; break;
      case Action::SOUND: p[1] = 0xA9; break;
      case Action::PREWASH: p[1] = 0x3B; break;
      case Action::AUTO_FLUSH: p[4] = (p[4] & ~2) | (value << 1); break;
      case Action::ION_CYCLE: p[1] = 0xF5; break;
      case Action::DELAY_CYCLE: p[1] = 0x53; break;
      case Action::POWER_SAVE: p[1] = 0xF1; break;
      case Action::HEATER_OFF: p[1] = 0xC9; break;
      case Action::FREEZE: p[1] = 0xD8; break;
      case Action::DESCALE: p[1] = 0x9A; break;
      case Action::RETRY_DRAIN: p[1] = 0xBA; break;
      case Action::SEAT_TEMP: {
        if (value < 0 || value > 5) return {};
        constexpr uint8_t codes[] = {0,1,3,5,6,7};
        p[4] = (p[4] & ~0x1C) | (codes[value] << 2); break;
      }
      case Action::DRY:
        if (value < 0 || value > 2) return {};
        p[1] = 0x5B + (value << 4); break;
      case Action::REAR: case Action::GENTLE: case Action::FRONT: {
        if (!wash.valid()) return {};
        constexpr uint8_t rear[] = {0x2E,0x3E,0x4E,0x5E,0x6E,0x7E};
        constexpr uint8_t gentle[] = {0x8E,0xAE,0x9E,0xBE,0xCE,0xEE};
        constexpr uint8_t front[] = {0xAD,0xBD,0xCD,0xDD,0xED,0xFD};
        constexpr uint8_t water[] = {1,3,4,5,6,7}, width[] = {1,3,2};
        constexpr uint8_t massage[] = {0,2,3,4,5,6};
        p[1] = (a == Action::REAR ? rear : a == Action::GENTLE ? gentle : front)[wash.pressure];
        p[2] = ((wash.pressure + 1) << 5) | (water[wash.water] << 2) |
               (a == Action::FRONT ? wash.width + 1 : width[wash.width]);
        p[3] |= (wash.position * 2 + 1) |
                (a == Action::FRONT ? 0 : massage[wash.massage] << 4);
        break;
      }
    }
    return p;
  }

};
inline Session session;
}  // namespace sensia
