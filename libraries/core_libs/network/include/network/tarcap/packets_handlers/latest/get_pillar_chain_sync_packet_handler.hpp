#pragma once

#include "common/packet_handler.hpp"
#include "pillar_chain/pillar_block.hpp"

namespace taraxa {
class DbStorage;
}

namespace taraxa::pillar_chain {
class PillarChainManager;
}

namespace taraxa::network::tarcap {

class GetPillarChainSyncPacketHandler : public PacketHandler {
 public:
  GetPillarChainSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                  std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<DbStorage> db,
                                  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                  const addr_t& node_addr, const std::string& logs_prefix);

  void requestPillarBlocks(PbftPeriod period, const std::shared_ptr<TaraxaPeer>& peer);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetPillarChainSyncPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kGetPillarChainSyncPacketSize{1};

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
