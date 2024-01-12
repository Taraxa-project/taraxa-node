#pragma once

#include "common/ext_bls_sig_packet_handler.hpp"

namespace taraxa::network::tarcap {

class BlsSigPacketHandler : public ExtBlsSigPacketHandler {
 public:
  BlsSigPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                      std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                      std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
                      const std::string& logs_prefix);

  void onNewBlsSig(const std::shared_ptr<pillar_chain::BlsSignature>& signature, bool rebroadcast = false);
  void sendBlsSig(const std::shared_ptr<TaraxaPeer>& peer,
                  const std::shared_ptr<pillar_chain::BlsSignature>& signature);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::BlsSigPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kBlsSigPacketSize{pillar_chain::BlsSignature::kRlpSize};
};

}  // namespace taraxa::network::tarcap
