#pragma once

#include <string>

#include "common/types.hpp"

namespace taraxa {

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

}