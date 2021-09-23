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
  NewBlockPacket,
  TransactionPacket,

  // Non critical packets with low processing priority
  LowPriorityPackets,
  TestPacket,
  StatusPacket,
  GetBlocksPacket,
  BlocksPacket,
  GetPbftBlockPacket,
  PbftBlockPacket,
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
    case NewBlockPacket:
      return "NewBlockPacket";
    case GetBlocksPacket:
      return "GetBlocksPacket";
    case BlocksPacket:
      return "BlocksPacket";
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
    case GetPbftBlockPacket:
      return "GetPbftBlockPacket";
    case PbftBlockPacket:
      return "PbftBlockPacket";
    case SyncedPacket:
      return "SyncedPacket";
    default:
      break;
  }

  return "Unknown packet type: " + std::to_string(packet_type);
}

}  // namespace taraxa::network::tarcap
