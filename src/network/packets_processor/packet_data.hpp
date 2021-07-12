#pragma once

#include <libdevcore/RLP.h>

namespace taraxa::network {

// TODO: use this type of enum values for SubprotocolPacketType
enum PacketType : uint32_t {
  // Consensus packets with high processing priority
  _HighPriorityPackets = 0,
  _PbftVotePacket = 1,
  _GetPbftNextVotes = 1 << 1,
  _PbftNextVotesPacket = 1 << 2,
  _NewPbftBlockPacket = 1 << 3,

  // Standard packets with mid processing priority
  _MidPriorityPackets = 1 << 4,
  _NewBlockPacket = 1 << 5,
  _NewBlockHashPacket = 1 << 6,
  _GetNewBlockPacket = 1 << 7,
  _TransactionPacket = 1 << 8,

  // Non critial packets with low processing priority
  _LowPriorityPackets = 1 << 9,
  _TestPacket = 1 << 10,
  _StatusPacket = 1 << 11,
  _GetBlocksPacket = 1 << 12,
  _BlocksPacket = 1 << 13,
  _GetPbftBlockPacket = 1 << 14,
  _PbftBlockPacket = 1 << 15,
  _SyncedPacket = 1 << 16,
  _SyncedResponsePacket = 1 << 17,
  _PacketCount
};

enum PacketPriority : size_t { High = 0, Mid, Low, Count };

using PacketData = struct {
  PacketType type;
  PacketPriority priority;
  dev::RLP rlp;
};

}  // namespace taraxa::network