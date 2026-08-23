#include <gtest/gtest.h>
#include "helpers/RoutingPolicy.h"

using namespace mesh;

static Packet makeFlood(uint8_t route_type, uint8_t payload_type, uint8_t hops) {
  Packet p;
  p.header = route_type | (payload_type << PH_TYPE_SHIFT);
  p.setPathHashSizeAndCount(1, hops);
  p.payload_len = 1;
  return p;
}

TEST(FloodHopLimit, AppliesUnscopedAndAdvertLimitsIndependently) {
  auto unscoped = makeFlood(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_RESPONSE, 0);
  EXPECT_TRUE(isFloodHopLimitExceeded(&unscoped, 64, 0, 8));

  auto scoped = makeFlood(ROUTE_TYPE_TRANSPORT_FLOOD, PAYLOAD_TYPE_RESPONSE, 0);
  EXPECT_FALSE(isFloodHopLimitExceeded(&scoped, 64, 0, 8));

  auto advert = makeFlood(ROUTE_TYPE_TRANSPORT_FLOOD, PAYLOAD_TYPE_ADVERT, 8);
  EXPECT_TRUE(isFloodHopLimitExceeded(&advert, 64, 64, 8));
}

TEST(ReplyRoute, UsesKnownDirectPathsBeforeFloodFallback) {
  EXPECT_EQ(REPLY_ROUTE_PATH_RETURN, chooseReplyRoute(true, false, true));
  EXPECT_EQ(REPLY_ROUTE_DIRECT_SUPPLIED, chooseReplyRoute(false, true, true));
  EXPECT_EQ(REPLY_ROUTE_DIRECT_OUT_PATH, chooseReplyRoute(false, false, true));
  EXPECT_EQ(REPLY_ROUTE_FLOOD, chooseReplyRoute(false, false, false));
}

TEST(ReplyScope, PreservesOrFallsBackToAUsableScope) {
  EXPECT_EQ(REPLY_SCOPE_REQUEST, chooseReplyScope(true, false, true));
  EXPECT_EQ(REPLY_SCOPE_NONE, chooseReplyScope(false, true, true));
  EXPECT_EQ(REPLY_SCOPE_DEFAULT, chooseReplyScope(false, false, true));
  EXPECT_EQ(REPLY_SCOPE_NONE, chooseReplyScope(false, false, false));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
