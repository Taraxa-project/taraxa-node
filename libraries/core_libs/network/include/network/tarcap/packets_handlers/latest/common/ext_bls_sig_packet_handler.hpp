#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class KeyManager;

namespace pillar_chain {
class BlsSignature;
class PillarChainManager;
}  // namespace pillar_chain

namespace final_chain {
class FinalChain;
}

}  // namespace taraxa

namespace taraxa::network::tarcap {

class ExtBlsSigPacketHandler : public PacketHandler {
 public:
  ExtBlsSigPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                         std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                         const addr_t& node_addr, const std::string& log_channel);

 protected:
  void processBlsSignature(const std::shared_ptr<pillar_chain::BlsSignature>& signature,
                           const std::shared_ptr<TaraxaPeer>& peer);

 protected:
  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
