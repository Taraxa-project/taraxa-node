#pragma once

#include <libp2p/Common.h>

#include <map>

#include "network/tarcap/packet_types.hpp"
#include "network/threadpool/packet_data.hpp"

namespace taraxa {
class PbftManager;
}

namespace taraxa::network::threadpool {

class PacketsBlockingMask {
 public:
  PacketsBlockingMask(const std::shared_ptr<PbftManager>& pbft_mgr);
  ~PacketsBlockingMask() = default;

  PacketsBlockingMask(const PacketsBlockingMask&) = default;
  PacketsBlockingMask& operator=(const PacketsBlockingMask&) = default;
  PacketsBlockingMask(PacketsBlockingMask&&) = default;
  PacketsBlockingMask& operator=(PacketsBlockingMask&&) = default;

  void markPacketAsHardBlocked(const PacketData& blocking_packet, SubprotocolPacketType packet_type_to_block);
  void markPacketAsHardUnblocked(const PacketData& blocking_packet, SubprotocolPacketType packet_type_to_unblock);

  void markPacketAsPeerOrderBlocked(const PacketData& blocking_packet, SubprotocolPacketType packet_type_to_block);
  void markPacketAsPeerOrderUnblocked(const PacketData& blocking_packet, SubprotocolPacketType packet_type_to_unblock);

  void setDagBlockLevelBeingProcessed(const PacketData& packet);
  void unsetDagBlockLevelBeingProcessed(const PacketData& packet);

  void setDagBlockBeingProcessed(const PacketData& packet);
  void unsetDagBlockBeingProcessed(const PacketData& packet);

  bool isPacketBlocked(const PacketData& packet_data) const;

 private:
  bool isPacketHardBlocked(const PacketData& packet_data) const;
  bool isPacketPeerOrderBlocked(const PacketData& packet_data) const;
  bool isDagBlockPacketBlockedByLevel(const PacketData& packet_data) const;
  bool isDagBlockPacketBlockedBySameDagBlock(const PacketData& packet_data) const;
  dev::RLP dagBlockFromDagPacket(const PacketData& packet_data) const;

  std::optional<taraxa::level_t> getSmallestDagLevelBeingProcessed() const;

 private:
  // Packets types that are currently hard blocked for processing in another threads due to dependencies,
  // e.g. syncing packets must be processed synchronously one by one, etc...
  // Each packet type might be simultaneously blocked by multiple different packets that are being processed. They
  // are saved in std::unordered_set<PacketData::PacketId> so hard block is released after the last blocking packet
  // is processed.
  std::unordered_map<SubprotocolPacketType, std::unordered_set<PacketData::PacketId>> hard_blocked_packet_types_;

  // Packets types that are blocked only for processing when received from specific peer & after specific
  // time (order), e.g.: new dag block packet processing is blocked until all transactions packets that were received
  // before it are processed. This blocking dependency is applied only for the same peer so transaction packet from one
  // peer does not block new dag block packet from another peer Order of blocks must be preserved, therefore using
  // std::set<PacketData::PacketId>
  std::unordered_map<SubprotocolPacketType, std::unordered_map<dev::p2p::NodeID, std::set<PacketData::PacketId>>>
      peer_order_blocked_packet_types_;

  // This "blocking dependency" is specific just for DagBlockPacket. Ideally only dag blocks with the same level
  // should be processed. In reality there are situation when node receives dag block with smaller level than the level
  // of blocks that are already being processed. In such case these blocks with smaller levels can be processed
  // concurrently with blocks that have higher level. All new dag blocks with higher level than the lowest level from
  // all the blocks that currently being processed are blocked for processing In this map are saved pairs of <level,
  // list>, where:
  //    level - What dag level have blocks that are currently being processed
  //    list - list of "DagBlockPacket" packets (of the same dag block level) that are currently being processed
  //    concurrently
  // Order of levels must be preserved, therefore using std::map
  std::map<taraxa::level_t, std::unordered_set<PacketData::PacketId>> processing_dag_levels_;

  // This "blocking dependency" is specific just for DagBlockPacket. Multiple nodes can send same dag blocks
  // concurrently, to reduce perofrmance impact only one packet/block will be processsed and others will be waiting.
  //  This map contains dag blocks that are currently processed with the associated packet id
  std::map<taraxa::sig_t, PacketData::PacketId> processing_dag_blocks_;

  std::shared_ptr<PbftManager> pbft_mgr_;

  static constexpr size_t kRequiredDagPacketSizeV3 = 2;
  static constexpr size_t kDagBlockPosV3 = 1;
  static constexpr size_t kRequiredDagPacketSizeV2 = 8;
};

}  // namespace taraxa::network::threadpool
