#include "sensia_protocol.h"
#include <cassert>
#include <iostream>

int main() {
  sensia::Session s;
  std::array<uint8_t, 20> p{};
  assert(s.receive(p.data(), 0) == sensia::Packet::INVALID);
  assert(s.receive(p.data(), 19) == sensia::Packet::INVALID);
  assert(s.receive(p.data(), 21) == sensia::Packet::INVALID);
  assert(!s.have_identity);
  assert(s.reply(1, 1, 0, 0).empty());

  p[3] = 0x12; p[4] = 0x34; p[5] = 0x56;
  p[6] = 0x78; p[7] = 0x9A; p[8] = 0xBC; p[9] = 1;
  assert(s.receive(p.data(), p.size()) == sensia::Packet::PAIRING);
  const std::vector<uint8_t> expected = {
    0, 0, 0, 0, 7, 0x97, 0xDF, 0xB0, 0, 0, 0x40, 0, 0,
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 1};
  assert(s.reply(10, 16, 24, 59).empty());
  auto frame = s.reply(10, 16, 23, 59);
  assert(frame == expected);
  assert(s.reply(0, 1, 0, 0).empty());
  s.pending_reply = false;

  p.fill(0); p[19] = 0x10; p[9] = 0x98; p[10] = 0x20;
  p[11] = 0x60; p[12] = 0x24; p[13] = 0xA8;
  assert(s.receive(p.data(), p.size()) == sensia::Packet::STATUS);
  assert(s.occupied && s.model == 2 && s.grade == 3);
  assert(s.pending_reply);
  frame = s.reply(1, 1, 0, 0);
  assert(frame[3] == 0x80 && frame[4] == 0xF7 && frame[19] == 2);
  assert(frame[13] == 0x12 && frame[18] == 0xBC);
  s.pending_reply = false;
  s.receive(p.data(), p.size());
  assert(!s.pending_reply);
  s = sensia::Session{};
  assert(!s.have_identity && !s.have_status && !s.pending_reply && !s.subscribed);
  p.fill(0); p[9] = 2;
  s.receive(p.data(), p.size());
  assert(s.phase == 2 && s.pending_reply);
  s.pending_reply = false;
  p[19] = 0x10;
  s.receive(p.data(), p.size());
  assert(!s.pending_reply);
  // Captured 39354SH0 status, 2026-10-19. Contains no pairing identity.
  const std::array<uint8_t, 20> captured = {
    0,0,0,0,0,1,0,0,0x51,0x91,0x0E,0x12,0,0x29,0,0xC1,0,0x12,0x90,0xFE};
  assert(s.receive(captured.data(), captured.size()) == sensia::Packet::STATUS);
  assert(s.have_status && !s.occupied && s.model == 2 && s.grade == 2);
  assert(s.settings == 0x06);
  using A = sensia::Action;
  sensia::WashProfile wash;
  auto command = [&](A a, int v = 0, bool maintenance = false) {
    return s.command(a, v, wash, 10, 19, 12, 30, maintenance);
  };
  assert(s.region == 2 && s.supports(A::BLUE_LED) && s.supports(A::DESCALE));
  assert(!s.supports(A::SEAT_TEMP) && !s.supports(A::FREEZE));
  assert(s.setting(A::AUTO_LID) == 0 && s.setting(A::DEODORIZER) == 1);
  assert(command(A::REAR).empty() && command(A::DRY).empty());
  assert(command(A::BLUE_LED, 1).empty()); // Already on: no toggle.
  assert(command(A::BLUE_LED, 0)[1] == 0x43);
  assert(command(A::AUTO_FLUSH, 0)[4] == 5);
  assert(command(A::DESCALE).empty());
  assert(command(A::DESCALE, 0, true)[1] == 0x9A);
  assert(command(A::RETRY_DRAIN, 0, true).empty());
  s.status[5] = 6;
  assert(command(A::RETRY_DRAIN, 0, true)[1] == 0xBA);
  assert(command(A::FLUSH_FULL).empty());
  s.status[5] = 0;
  s.occupied = true;
  assert(command(A::DESCALE, 0, true).empty());
  const uint8_t gentle[] = {0x8E,0xAE,0x9E,0xBE,0xCE,0xEE};
  for (int pressure = 0; pressure < 6; ++pressure) {
    wash.pressure = pressure;
    assert(command(A::GENTLE)[1] == gentle[pressure]);
    assert(command(A::REAR)[1] == 0x2E + pressure * 16);
    assert(command(A::FRONT)[1] == 0xAD + pressure * 16);
  }
  wash = sensia::WashProfile{};
  s.drainage = 0x80;
  auto rear = command(A::REAR);
  assert(rear[1] == 0x4E && rear[2] == 0x6D && rear[3] == 0x85);
  assert(rear[4] == 7 && rear[19] == 2 && rear[13] == s.identity[0]);
  wash.width = 1; wash.massage = 5;
  assert(command(A::REAR)[2] == 0x6F && command(A::REAR)[3] == 0xE5);
  assert(command(A::FRONT)[2] == 0x6E && command(A::FRONT)[3] == 0x85);
  wash.pressure = -1; assert(command(A::REAR).empty());
  wash.pressure = 6; assert(command(A::REAR).empty());
  wash = sensia::WashProfile{};
  assert(command(A::DRY, 0)[1] == 0x5B && command(A::DRY, 2)[1] == 0x7B);
  assert(command(A::DRY, 3).empty());
  for (int model = 0; model < 4; ++model) {
    for (int region = 0; region < 8; ++region) {
      s.model = model; s.region = region;
      bool known = model >= 2 && region >= 1 && region <= 4;
      assert(s.supports(A::STOP) == known);
      assert(s.supports(A::SEAT_TEMP) == (model == 2 && (region == 3 || region == 4)));
      assert(s.supports(A::FREEZE) == (model == 2 && region == 3));
      assert(s.supports(A::BLUE_LED) == (model == 2 && region >= 2 && region <= 4));
      assert(s.supports(A::DESCALE) == (model == 2 && (region == 2 || region == 4)));
    }
  }
  s.model = 2; s.region = 4;
  for (int grade = 0; grade < 8; ++grade) {
    s.grade = grade;
    assert(s.supports(A::OPEN_LID) == (grade == 2 || grade == 3));
  }
  s.grade = 2;
  assert(command(A::SEAT_TEMP, 1).empty()); // Already reported: do not wait for a change.
  s.settings = 0xE6;
  auto heater = command(A::SEAT_TEMP, 5);
  assert(heater[4] == 0xFF && heater[3] == 0x80);
  assert(command(A::SEAT_TEMP, 6).empty());
  s = sensia::Session{};
  assert(command(A::STOP).empty());
  std::cout << "Protocol checks passed\n";
}
