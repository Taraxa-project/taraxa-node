#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa::network {

/**
 * @brief SubprotocolPacketType is used in networking layer to differentiate packet types
 */
enum SubprotocolPacketType : uint32_t {
  // Consensus packets with high processing priority
  HighPriorityPackets = 0,
  VotePacket,  // Vote packer can contain (optional) also pbft block
  GetNextVotesSyncPacket,
  VotesBundlePacket,

  // Standard packets with mid processing priority
  MidPriorityPackets,
  DagBlockPacket,
  // DagSyncPacket has mid priority as it is also used for ad-hoc syncing in case new dag blocks miss tips/pivot
  DagSyncPacket,
  TransactionPacket,

  // Non critical packets with low processing priority
  LowPriorityPackets,
  StatusPacket,
  GetPbftSyncPacket,
  PbftSyncPacket,
  GetDagSyncPacket,
  PillarVotePacket,
  GetPillarVotesBundlePacket,
  PillarVotesBundlePacket,

  PacketCount
};

/**
 * @param packet_type
 * @return string representation of packet_type
 */
inline std::string convertPacketTypeToString(SubprotocolPacketType packet_type) {
  switch (packet_type) {
    case StatusPacket:
      return "StatusPacket";
    case DagBlockPacket:
      return "DagBlockPacket";
    case GetDagSyncPacket:
      return "GetDagSyncPacket";
    case DagSyncPacket:
      return "DagSyncPacket";
    case TransactionPacket:
      return "TransactionPacket";
    case VotePacket:
      return "VotePacket";
    case GetNextVotesSyncPacket:
      return "GetNextVotesSyncPacket";
    case VotesBundlePacket:
      return "VotesBundlePacket";
    case GetPbftSyncPacket:
      return "GetPbftSyncPacket";
    case PbftSyncPacket:
      return "PbftSyncPacket";
    case PillarVotePacket:
      return "PillarVotePacket";
    case GetPillarVotesBundlePacket:
      return "GetPillarVotesBundlePacket";
    case PillarVotesBundlePacket:
      return "PillarVotesBundlePacket";
    default:
      break;
  }

  return "Unknown packet type: " + std::to_string(packet_type);
}

}  // namespace taraxa::network
