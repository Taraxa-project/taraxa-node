#pragma once

#include "network/tarcap/packets/latest/get_pillar_votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/interface/get_pillar_votes_bundle_packet_handler.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

class GetPillarVotesBundlePacketHandler : public IGetPillarVotesBundlePacketHandler {
 public:
  GetPillarVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                    const addr_t& node_addr, const std::string& logs_prefix = "");

  void requestPillarVotesBundle(PbftPeriod period, const blk_hash_t& pillar_block_hash,
                                const std::shared_ptr<TaraxaPeer>& peer) override;

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kGetPillarVotesBundlePacket;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 protected:
  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager_;
};

}  // namespace taraxa::network::tarcap
