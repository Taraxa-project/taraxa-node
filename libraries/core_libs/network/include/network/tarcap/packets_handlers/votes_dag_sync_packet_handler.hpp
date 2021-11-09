#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
class FinalChain;
class PbftChain;
class DbStorage;
}  // namespace taraxa

namespace taraxa::final_chain {
class FinalChain;
}  // namespace taraxa::final_chain

namespace taraxa::network::tarcap {

// TODO: this handler might be merged with VotesSyncPacketHandler - not touching it atm as it should be properly
// refactored
class VotesDagSyncPacketHandler : public ExtVotesPacketHandler {
 public:
  VotesDagSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                            std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<VoteManager> vote_mgr,
                            std::shared_ptr<final_chain::FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                            std::shared_ptr<DbStorage> db, const addr_t& node_addr);

  virtual ~VotesDagSyncPacketHandler() = default;

 private:
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
