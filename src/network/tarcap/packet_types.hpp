#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief SubprotocolPacketType is used in networking layer to differentiate packet types
 */
enum SubprotocolPacketType : uint32_t {
  // Consensus packets with high processing priority
  HighPriorityPackets = 0,
  PbftVotePacket,
  GetPbftNextVotes,
  PbftNextVotesPacket,

  // Standard packets with mid processing priority
  MidPriorityPackets,
  NewPbftBlockPacket,
  NewDagBlockPacket,
  // DagBlocksSyncPacket has mid priority as it is also used for ad-hoc syncing in case new dag blocks miss tips/pivot
  DagBlocksSyncPacket,
  TransactionPacket,

  // Non critical packets with low processing priority
  LowPriorityPackets,
  TestPacket,
  StatusPacket,
  GetPbftBlocksSyncPacket,
  PbftBlocksSyncPacket,
  GetDagBlocksSyncPacket,
  SyncedPacket,

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
    case NewDagBlockPacket:
      return "NewDagBlockPacket";
    case GetDagBlocksSyncPacket:
      return "GetDagBlocksSyncPacket";
    case DagBlocksSyncPacket:
      return "DagBlocksSyncPacket";
    case TransactionPacket:
      return "TransactionPacket";
    case TestPacket:
      return "TestPacket";
    case PbftVotePacket:
      return "PbftVotePacket";
    case GetPbftNextVotes:
      return "GetPbftNextVotes";
    case PbftNextVotesPacket:
      return "PbftNextVotesPacket";
    case NewPbftBlockPacket:
      return "NewPbftBlockPacket";
    case GetPbftBlocksSyncPacket:
      return "GetPbftBlocksSyncPacket";
    case PbftBlocksSyncPacket:
      return "PbftBlocksSyncPacket";
    case SyncedPacket:
      return "SyncedPacket";
    default:
      break;
  }

  return "Unknown packet type: " + std::to_string(packet_type);
}

}  // namespace taraxa::network::tarcap
