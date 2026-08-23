#pragma once

#include <Packet.h>

namespace mesh {

/**
 * \\brief  Test a flood packet against the configured hop limits.
 * \\param  packet  inbound flood packet (caller has already checked isRouteFlood())
 * \\param  flood_max            max hops for any flood packet
 * \\param  flood_max_unscoped   max hops for ROUTE_TYPE_FLOOD (ie. un-scoped) packets
 * \\param  flood_max_advert     max hops for ADVERT packets
 * \\returns  true if the packet has exceeded a limit, and must not be forwarded
 */
inline bool isFloodHopLimitExceeded(const Packet* packet, uint8_t flood_max,
                                    uint8_t flood_max_unscoped, uint8_t flood_max_advert) {
  uint8_t hops = packet->getPathHashCount();
  if (hops >= flood_max) return true;
  if (packet->getRouteType() == ROUTE_TYPE_FLOOD && hops >= flood_max_unscoped) return true;
  if (packet->getPayloadType() == PAYLOAD_TYPE_ADVERT && hops >= flood_max_advert) return true;
  return false;
}

/** \\brief How a server routes a reply back to the requesting client. */
enum ReplyRoute : uint8_t {
  REPLY_ROUTE_PATH_RETURN,
  REPLY_ROUTE_DIRECT_SUPPLIED,
  REPLY_ROUTE_DIRECT_OUT_PATH,
  REPLY_ROUTE_FLOOD,
};

inline ReplyRoute chooseReplyRoute(bool inbound_is_flood, bool have_supplied_path, bool have_out_path) {
  if (inbound_is_flood) return REPLY_ROUTE_PATH_RETURN;
  if (have_supplied_path) return REPLY_ROUTE_DIRECT_SUPPLIED;
  if (have_out_path) return REPLY_ROUTE_DIRECT_OUT_PATH;
  return REPLY_ROUTE_FLOOD;
}

/** \\brief Which transport scope a flooded reply should use. */
enum ReplyScope : uint8_t {
  REPLY_SCOPE_REQUEST,
  REPLY_SCOPE_DEFAULT,
  REPLY_SCOPE_NONE,
};

inline ReplyScope chooseReplyScope(bool request_scope_known, bool request_was_unscoped_flood,
                                   bool default_scope_known) {
  if (request_scope_known) return REPLY_SCOPE_REQUEST;
  if (request_was_unscoped_flood) return REPLY_SCOPE_NONE;
  if (default_scope_known) return REPLY_SCOPE_DEFAULT;
  return REPLY_SCOPE_NONE;
}

}  // namespace mesh
