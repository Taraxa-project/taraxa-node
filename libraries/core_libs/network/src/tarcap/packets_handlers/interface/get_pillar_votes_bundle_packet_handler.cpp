#include "network/tarcap/packets_handlers/interface/get_pillar_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

IGetPillarVotesBundlePacketHandler::IGetPillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, const addr_t &node_addr, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, logs_prefix) {}

}  // namespace taraxa::network::tarcap
