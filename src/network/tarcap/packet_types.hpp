#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief SubprotocolPacketType is used in networking layer to differentiate packet types
 */
enum SubprotocolPacketType : uint32_t {
  StatusPacket = 0,
  NewBlockPacket,
  NewBlockHashPacket,
  GetNewBlockPacket,
  GetBlocksPacket,
  BlocksPacket,
  TransactionPacket,
  TestPacket,
  PbftVotePacket,
  GetPbftNextVotes,
  PbftNextVotesPacket,
  NewPbftBlockPacket,
  GetPbftBlockPacket,
  PbftBlockPacket,
  SyncedPacket,
  PacketCount
};

/**
 * @brief PriorityQueuePacketType is mapped 1:1 to SubprotocolPacketType. It is used inside tarcap priority queue for
 *        more efficient processing of different packets types dependencies thanks to it's bit mask representation
 */
enum PriorityQueuePacketType : uint32_t {
  // Consensus packets with high processing priority
  PQ_HighPriorityPackets = 0,
  PQ_PbftVotePacket = 1,
  PQ_GetPbftNextVotes = 1 << 1,
  PQ_PbftNextVotesPacket = 1 << 2,

  // Standard packets with mid processing priority
  PQ_MidPriorityPackets = 1 << 3,
  PQ_NewPbftBlockPacket = 1 << 4,
  PQ_NewBlockPacket = 1 << 5,
  PQ_NewBlockHashPacket = 1 << 6,
  PQ_GetNewBlockPacket = 1 << 7,
  PQ_TransactionPacket = 1 << 8,

  // Non critical packets with low processing priority
  PQ_LowPriorityPackets = 1 << 9,
  PQ_TestPacket = 1 << 10,
  PQ_StatusPacket = 1 << 11,
  PQ_GetBlocksPacket = 1 << 12,
  PQ_BlocksPacket = 1 << 13,
  PQ_GetPbftBlockPacket = 1 << 14,
  PQ_PbftBlockPacket = 1 << 15,
  PQ_SyncedPacket = 1 << 16,
};

/**
 * @param packet_type
 * @return PriorityQueuePacketType corresponding to packet_type (SubprotocolPacketType)
 */
inline PriorityQueuePacketType mapSubProtocolToPriorityPacketType(SubprotocolPacketType packet_type) {
  switch (packet_type) {
    case SubprotocolPacketType::StatusPacket:
      return PriorityQueuePacketType::PQ_StatusPacket;
    case SubprotocolPacketType::NewBlockPacket:
      return PriorityQueuePacketType::PQ_NewBlockPacket;
    case SubprotocolPacketType::NewBlockHashPacket:
      return PriorityQueuePacketType::PQ_NewBlockHashPacket;
    case SubprotocolPacketType::GetNewBlockPacket:
      return PriorityQueuePacketType::PQ_GetNewBlockPacket;
    case SubprotocolPacketType::GetBlocksPacket:
      return PriorityQueuePacketType::PQ_GetBlocksPacket;
    case SubprotocolPacketType::BlocksPacket:
      return PriorityQueuePacketType::PQ_BlocksPacket;
    case SubprotocolPacketType::TransactionPacket:
      return PriorityQueuePacketType::PQ_TransactionPacket;
    case SubprotocolPacketType::TestPacket:
      return PriorityQueuePacketType::PQ_TestPacket;
    case SubprotocolPacketType::PbftVotePacket:
      return PriorityQueuePacketType::PQ_PbftVotePacket;
    case SubprotocolPacketType::GetPbftNextVotes:
      return PriorityQueuePacketType::PQ_GetPbftNextVotes;
    case SubprotocolPacketType::PbftNextVotesPacket:
      return PriorityQueuePacketType::PQ_PbftNextVotesPacket;
    case SubprotocolPacketType::NewPbftBlockPacket:
      return PriorityQueuePacketType::PQ_NewPbftBlockPacket;
    case SubprotocolPacketType::GetPbftBlockPacket:
      return PriorityQueuePacketType::PQ_GetPbftBlockPacket;
    case SubprotocolPacketType::PbftBlockPacket:
      return PriorityQueuePacketType::PQ_PbftBlockPacket;
    case SubprotocolPacketType::SyncedPacket:
      return PriorityQueuePacketType::PQ_SyncedPacket;
    default:
      break;
  }

  assert(false);
  throw std::runtime_error("Unknown packet type: " + std::to_string(packet_type));
}

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