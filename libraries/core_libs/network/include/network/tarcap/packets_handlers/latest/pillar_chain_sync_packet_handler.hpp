#pragma once

#include "common/packet_handler.hpp"

namespace taraxa::pillar_chain {
class PillarChainManager;
}

namespace taraxa::network::tarcap {

class PillarChainSyncPacketHandler : public PacketHandler {
 public:
  PillarChainSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                               std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                               const addr_t& node_addr, const std::string& logs_prefix);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PillarChainSyncPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kPillarChainSyncPacketSize{1};

  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
