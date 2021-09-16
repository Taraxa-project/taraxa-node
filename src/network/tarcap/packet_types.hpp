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
  kPqHighPriorityPackets = 0,
  kPqPbftVotePacket = 1,
  kPqGetPbftNextVotes = 1 << 1,
  kPqPbftNextVotesPacket = 1 << 2,

  // Standard packets with mid processing priority
  kPqMidPriorityPackets = 1 << 3,
  kPqNewPbftBlockPacket = 1 << 4,
  kPqNewBlockPacket = 1 << 5,
  kPqNewBlockHashPacket = 1 << 6,
  kPqGetNewBlockPacket = 1 << 7,
  kPqTransactionPacket = 1 << 8,

  // Non critical packets with low processing priority
  kPqLowPriorityPackets = 1 << 9,
  kPqTestPacket = 1 << 10,
  kPqStatusPacket = 1 << 11,
  kPqGetBlocksPacket = 1 << 12,
  kPqBlocksPacket = 1 << 13,
  kPqGetPbftBlockPacket = 1 << 14,
  kPqPbftBlockPacket = 1 << 15,
  kPqSyncedPacket = 1 << 16,
};

/**
 * @param packet_type
 * @return PriorityQueuePacketType corresponding to packet_type (SubprotocolPacketType)
 */
inline PriorityQueuePacketType mapSubProtocolToPriorityPacketType(SubprotocolPacketType packet_type) {
  switch (packet_type) {
    case SubprotocolPacketType::StatusPacket:
      return PriorityQueuePacketType::kPqStatusPacket;
    case SubprotocolPacketType::NewBlockPacket:
      return PriorityQueuePacketType::kPqNewBlockPacket;
    case SubprotocolPacketType::NewBlockHashPacket:
      return PriorityQueuePacketType::kPqNewBlockHashPacket;
    case SubprotocolPacketType::GetNewBlockPacket:
      return PriorityQueuePacketType::kPqGetNewBlockPacket;
    case SubprotocolPacketType::GetBlocksPacket:
      return PriorityQueuePacketType::kPqGetBlocksPacket;
    case SubprotocolPacketType::BlocksPacket:
      return PriorityQueuePacketType::kPqBlocksPacket;
    case SubprotocolPacketType::TransactionPacket:
      return PriorityQueuePacketType::kPqTransactionPacket;
    case SubprotocolPacketType::TestPacket:
      return PriorityQueuePacketType::kPqTestPacket;
    case SubprotocolPacketType::PbftVotePacket:
      return PriorityQueuePacketType::kPqPbftVotePacket;
    case SubprotocolPacketType::GetPbftNextVotes:
      return PriorityQueuePacketType::kPqGetPbftNextVotes;
    case SubprotocolPacketType::PbftNextVotesPacket:
      return PriorityQueuePacketType::kPqPbftNextVotesPacket;
    case SubprotocolPacketType::NewPbftBlockPacket:
      return PriorityQueuePacketType::kPqNewPbftBlockPacket;
    case SubprotocolPacketType::GetPbftBlockPacket:
      return PriorityQueuePacketType::kPqGetPbftBlockPacket;
    case SubprotocolPacketType::PbftBlockPacket:
      return PriorityQueuePacketType::kPqPbftBlockPacket;
    case SubprotocolPacketType::SyncedPacket:
      return PriorityQueuePacketType::kPqSyncedPacket;
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