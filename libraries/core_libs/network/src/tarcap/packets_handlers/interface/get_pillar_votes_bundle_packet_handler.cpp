#include "network/tarcap/packets_handlers/interface/get_pillar_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

IGetPillarVotesBundlePacketHandler::IGetPillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, const std::string &log_channel_name)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), log_channel_name) {}

}  // namespace taraxa::network::tarcap
