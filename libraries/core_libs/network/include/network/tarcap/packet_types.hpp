#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa::network {

/**
 * @brief SubprotocolPacketType is used in networking layer to differentiate packet types
 */
enum SubprotocolPacketType : uint32_t {
  // Consensus packets with high processing priority
  kHighPriorityPackets = 0,
  kVotePacket,  // Vote packer can contain (optional) also pbft block
  kGetNextVotesSyncPacket,
  kVotesBundlePacket,

  // Standard packets with mid processing priority
  kMidPriorityPackets,
  kDagBlockPacket,
  // DagSyncPacket has mid priority as it is also used for ad-hoc syncing in case new dag blocks miss tips/pivot
  kDagSyncPacket,
  kTransactionPacket,

  // Non critical packets with low processing priority
  kLowPriorityPackets,
  kStatusPacket,
  kGetPbftSyncPacket,
  kPbftSyncPacket,
  kGetDagSyncPacket,
  kPillarVotePacket,
  kGetPillarVotesBundlePacket,
  kPillarVotesBundlePacket,

  kPacketCount
};

/**
 * @param packet_type
 * @return string representation of packet_type
 */
inline std::string convertPacketTypeToString(SubprotocolPacketType packet_type) {
  switch (packet_type) {
    case kStatusPacket:
      return "StatusPacket";
    case kDagBlockPacket:
      return "DagBlockPacket";
    case kGetDagSyncPacket:
      return "GetDagSyncPacket";
    case kDagSyncPacket:
      return "DagSyncPacket";
    case kTransactionPacket:
      return "TransactionPacket";
    case kVotePacket:
      return "VotePacket";
    case kGetNextVotesSyncPacket:
      return "GetNextVotesSyncPacket";
    case kVotesBundlePacket:
      return "VotesBundlePacket";
    case kGetPbftSyncPacket:
      return "GetPbftSyncPacket";
    case kPbftSyncPacket:
      return "PbftSyncPacket";
    case kPillarVotePacket:
      return "PillarVotePacket";
    case kGetPillarVotesBundlePacket:
      return "GetPillarVotesBundlePacket";
    case kPillarVotesBundlePacket:
      return "PillarVotesBundlePacket";
    default:
      break;
  }

  return "Unknown packet type: " + std::to_string(packet_type);
}

}  // namespace taraxa::network
