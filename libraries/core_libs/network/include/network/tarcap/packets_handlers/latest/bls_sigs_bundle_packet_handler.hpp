#pragma once

#include "common/ext_bls_sig_packet_handler.hpp"

namespace taraxa::network::tarcap {

class BlsSigsBundlePacketHandler : public ExtBlsSigPacketHandler {
 public:
  BlsSigsBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                             std::shared_ptr<PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
                             const std::string& logs_prefix);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::BlsSigsBundlePacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  constexpr static size_t kMaxSignaturesInBundleRlp{250};
};

}  // namespace taraxa::network::tarcap
