#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Derived from APK 1.4.1 BLEManager and NToiletInfoManager; not hardware validated.
namespace sensia {
enum class Packet { INVALID, PAIRING, STATUS };

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

  Packet receive(const uint8_t *p, size_t size) {
    if (size != 20) return Packet::INVALID;
    if ((p[19] & 0xF0) == 0) {
      for (size_t i = 0; i < identity.size(); ++i) identity[i] = p[i + 3];
      have_identity = true;
      phase = p[9] == 1 ? 1 : 2;
      pending_reply = true;
      return Packet::PAIRING;
    }
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
};
inline Session session;
}  // namespace sensia
