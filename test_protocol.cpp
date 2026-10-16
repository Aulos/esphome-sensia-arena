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
  std::cout << "Protocol checks passed\n";
}
