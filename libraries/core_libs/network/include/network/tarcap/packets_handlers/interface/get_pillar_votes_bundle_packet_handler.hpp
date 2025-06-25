#pragma once

#include "network/tarcap/packets_handlers/latest/common/packet_handler.hpp"

namespace taraxa::network::tarcap {

class IGetPillarVotesBundlePacketHandler : public PacketHandler {
 public:
  IGetPillarVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                     const std::string& log_channel_name);

  virtual void requestPillarVotesBundle(PbftPeriod period, const blk_hash_t& pillar_block_hash,
                                        const std::shared_ptr<TaraxaPeer>& peer) = 0;
};

}  // namespace taraxa::network::tarcap
