#include "network/tarcap/packets_handlers/get_votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

GetVotesSyncPacketHandler::GetVotesSyncPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
    std::shared_ptr<NextVotesManager> next_votes_mgr, const NetworkConfig &net_config, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), net_config, node_addr, "GET_VOTES_SYNC_PH"),
      next_votes_mgr_(std::move(next_votes_mgr)) {}

void GetVotesSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 3; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetVotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "Received GetVotesSyncPacket request";

  const PbftPeriod peer_pbft_period = packet_data.rlp_[0].toInt<PbftPeriod>();
  const PbftRound peer_pbft_round = packet_data.rlp_[1].toInt<PbftRound>();
  const size_t peer_pbft_previous_round_next_votes_size = packet_data.rlp_[2].toInt<unsigned>();
  const auto [pbft_round, pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
  const size_t pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();

  if (pbft_period == peer_pbft_period &&
      (pbft_round > peer_pbft_round ||
       (pbft_round == peer_pbft_round &&
        pbft_previous_round_next_votes_size > peer_pbft_previous_round_next_votes_size))) {
    LOG(log_dg_) << "In PBFT period " << pbft_period << ", current PBFT round is " << pbft_round
                 << " previous round next votes size " << pbft_previous_round_next_votes_size
                 << ", and peer PBFT round is " << peer_pbft_round << " previous round next votes size "
                 << peer_pbft_previous_round_next_votes_size << ". Will send out bundle of next votes";

    // TODO: send also a block
    auto next_votes_bundle = next_votes_mgr_->getNextVotes();
    std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
    for (auto &v : next_votes_bundle) {
      if (!peer->isVoteKnown(v->getHash())) {
        send_next_votes_bundle.push_back(std::move(v));
      }
    }
    sendPbftVotes(peer, std::move(send_next_votes_bundle), true);
  }
}

}  // namespace taraxa::network::tarcap
