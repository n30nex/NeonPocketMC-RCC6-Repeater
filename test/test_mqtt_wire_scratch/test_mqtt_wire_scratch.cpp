// Boundary tests for the wire-format scratch sizing used by the MQTT raw/packet
// publish paths. Packet::writeTo() cannot report an overrun (uint8_t return) and
// trusts the packet's own length fields, so canSerialize() is what keeps it in
// bounds — these cases pin the exact accept/reject edges.
#include <gtest/gtest.h>

#include "helpers/MQTTWireScratch.h"

namespace {

// A packet that serializes to the largest legal wire form: transport codes present,
// a full path, and a full payload.
mesh::Packet maxPacket() {
  mesh::Packet p;
  p.header = ROUTE_TYPE_TRANSPORT_DIRECT | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  p.transport_codes[0] = 0x1234;
  p.transport_codes[1] = 0x5678;
  // The hop count field is 6 bits, so MAX_PATH_SIZE one-byte hops is NOT encodable
  // (64 & 63 == 0). 32 hops of 2 bytes is the widest path that reaches MAX_PATH_SIZE.
  p.setPathHashSizeAndCount(2, 32);
  EXPECT_EQ(MAX_PATH_SIZE, p.getPathByteLen());
  p.payload_len = MAX_PACKET_PAYLOAD;
  memset(p.payload, 0xAB, sizeof(p.payload));
  return p;
}

}  // namespace

TEST(MQTTWireScratch, MaxLegalPacketFitsTheScratchBuffer) {
  mesh::Packet p = maxPacket();
  // 1 header + 4 transport + 1 path_len + 64 path + 184 payload = 254.
  EXPECT_EQ(254, p.getRawLength());
  EXPECT_TRUE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));

  uint8_t buf[MQTTWireScratch::kWireBytes];
  const uint8_t written = p.writeTo(buf);
  EXPECT_EQ(254, (int)written);
  EXPECT_LE((size_t)written, sizeof(buf));
  // The hex buffer must hold two chars per byte plus the NUL.
  EXPECT_GE(MQTTWireScratch::kWireHexChars, (size_t)written * 2 + 1);
}

TEST(MQTTWireScratch, RejectsPayloadLenPastTheArrayEvenWhenEncodedLengthFits) {
  mesh::Packet p;
  p.header = ROUTE_TYPE_FLOOD;
  p.setPathHashSizeAndCount(1, 0);
  // getRawLength() == 2 + 0 + 185 == 187, comfortably inside MAX_TRANS_UNIT, but
  // writeTo() would memcpy 185 bytes out of a 184-byte array.
  p.payload_len = MAX_PACKET_PAYLOAD + 1;
  EXPECT_LE(p.getRawLength(), (int)MQTTWireScratch::kWireBytes);
  EXPECT_FALSE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));

  p.payload_len = MAX_PACKET_PAYLOAD;
  EXPECT_TRUE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));
}

TEST(MQTTWireScratch, RejectsPathLenThatWouldTruncateIntoOneWireByte) {
  mesh::Packet p;
  p.header = ROUTE_TYPE_FLOOD;
  p.payload_len = 4;
  p.path_len = 0x100;  // writeTo() stores this in a single byte
  EXPECT_FALSE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));

  p.path_len = 0xFF;
  // Still rejected, but now on the destination check rather than truncation:
  // 0xFF encodes 63 hops of 4 bytes.
  EXPECT_GT(p.getRawLength(), (int)MQTTWireScratch::kWireBytes);
  EXPECT_FALSE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));
}

TEST(MQTTWireScratch, DestinationEdgeIsInclusive) {
  mesh::Packet p = maxPacket();
  const size_t exact = (size_t)p.getRawLength();
  EXPECT_TRUE(MQTTWireScratch::canSerialize(p, exact));
  EXPECT_FALSE(MQTTWireScratch::canSerialize(p, exact - 1));
}

// A zero-payload packet serializes fine but does not survive readFrom(), which
// requires at least one payload byte. canSerialize() deliberately does not reject it:
// the raw/packet publish paths only ever write the bytes out. Any future change that
// reconstructs a Packet from queued wire bytes has to handle this asymmetry.
TEST(MQTTWireScratch, ZeroPayloadPacketSerializesButDoesNotRoundTrip) {
  mesh::Packet p;
  p.header = ROUTE_TYPE_FLOOD;
  p.setPathHashSizeAndCount(1, 0);
  p.payload_len = 0;
  EXPECT_EQ(2, p.getRawLength());
  EXPECT_TRUE(MQTTWireScratch::canSerialize(p, MQTTWireScratch::kWireBytes));

  uint8_t buf[MQTTWireScratch::kWireBytes];
  const uint8_t written = p.writeTo(buf);
  EXPECT_EQ(2, (int)written);

  mesh::Packet restored;
  EXPECT_FALSE(restored.readFrom(buf, written));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
