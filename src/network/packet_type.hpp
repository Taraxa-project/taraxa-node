#pragma once

#include <string>

enum SubprotocolPacketType : ::byte {
  StatusPacket = 0x0,
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
  SyncedResponsePacket,
  PacketCount
};

static std::string packetToPacketName(const ::byte &packet_type) {
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
    case PacketCount:
      return "PacketCount";
    case SyncedPacket:
      return "SyncedPacket";
    case SyncedResponsePacket:
      return "SyncedResponsePacket";
  }

  return "Unknown packet(byteIdx: " + std::to_string(packet_type) + ")";
}