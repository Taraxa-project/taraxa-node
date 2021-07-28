#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa::network::tarcap {

// TODO: seems like packet types must be implemented as 1,2,3,4... to actually work with aleth - check it !!!
//       In such case new enum with mask value must be implemented
// !!! Important: do not forget to adjust packet_types_count() in case new packet type is added
enum SubprotocolPacketType : unsigned /* aleth uses unsigned for packet id */ {
  // Consensus packets with high processing priority
  HighPriorityPackets = 0,
  PbftVotePacket = 1,
  GetPbftNextVotes = 1 << 1,
  PbftNextVotesPacket = 1 << 2,

  // Standard packets with mid processing priority
  MidPriorityPackets = 1 << 3,
  NewPbftBlockPacket = 1 << 4,
  NewBlockPacket = 1 << 5,
  NewBlockHashPacket = 1 << 6,
  GetNewBlockPacket = 1 << 7,
  TransactionPacket = 1 << 8,

  // Non critical packets with low processing priority
  LowPriorityPackets = 1 << 9,
  TestPacket = 1 << 10,
  StatusPacket = 1 << 11,
  GetBlocksPacket = 1 << 12,
  BlocksPacket = 1 << 13,
  GetPbftBlockPacket = 1 << 14,
  PbftBlockPacket = 1 << 15,
  SyncedPacket = 1 << 16,
};

/**
 * @return number of real packet types defined in SubprotocolPacketType
 */
inline size_t packets_types_count() { return 16; }

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
    case NewBlockHashPacket:
      return "NewBlockHashPacket";
    case GetNewBlockPacket:
      return "GetNewBlockPacket";
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