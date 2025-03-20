#pragma once

#include "common/packet_handler.hpp"
#include "network/tarcap/packets/latest/pbft_blocks_bundle_packet.hpp"

namespace taraxa {
class PbftManager;
}

namespace taraxa::network::tarcap {

class PbftSyncingState;

class PbftBlocksBundlePacketHandler : public PacketHandler<PbftBlocksBundlePacket> {
 public:
  PbftBlocksBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftSyncingState> syncing_state,
                                const addr_t& node_addr, const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPbftBlocksBundlePacket;

 private:
  virtual void process(PbftBlocksBundlePacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
};

}  // namespace taraxa::network::tarcap
