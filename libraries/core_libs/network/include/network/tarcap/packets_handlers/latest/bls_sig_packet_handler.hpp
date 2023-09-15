#pragma once

#include "common/packet_handler.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

class BlsSigPacketHandler : public PacketHandler {
 public:
  BlsSigPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                      std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                      std::shared_ptr<PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
                      const std::string& logs_prefix);

  void onNewBlsSig(const std::shared_ptr<BlsSignature>& signature, bool rebroadcast = false);
  void sendBlsSig(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<BlsSignature>& signature);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::BlsSigPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kBlsSigPacketSize{1};

  std::shared_ptr<PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
