#pragma once

#include "common/packet_handler.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

class GetBlsSigsBundlePacketHandler : public PacketHandler {
 public:
  GetBlsSigsBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                std::shared_ptr<PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
                                const std::string& logs_prefix);

  void requestBlsSigsBundle(const PillarBlock::Hash& pillar_block_hash, const std::shared_ptr<TaraxaPeer>& peer);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetBlsSigsBundlePacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kGetBlsSigsPacketSize{1};

  std::shared_ptr<PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
