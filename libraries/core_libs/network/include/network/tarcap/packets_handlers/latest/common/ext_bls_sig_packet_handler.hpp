#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class KeyManager;
class BlsSignature;
class PillarChainManager;

namespace final_chain {
class FinalChain;
}

}  // namespace taraxa

namespace taraxa::network::tarcap {

class ExtBlsSigPacketHandler : public PacketHandler {
 public:
  ExtBlsSigPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                         std::shared_ptr<PillarChainManager> pillar_chain_manager, const addr_t& node_addr,
                         const std::string& log_channel);

 protected:
  void processBlsSignature(const std::shared_ptr<BlsSignature>& signature, const std::shared_ptr<TaraxaPeer>& peer);

 protected:
  std::shared_ptr<PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
