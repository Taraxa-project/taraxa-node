#include "network/tarcap/packets_handlers/get_next_votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

GetNextVotesSyncPacketHandler::GetNextVotesSyncPacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
    std::shared_ptr<NextVotesManager> next_votes_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "GET_NEXT_VOTES_SYNC_PH"),
      next_votes_mgr_(std::move(next_votes_mgr)) {}

void GetNextVotesSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetNextVotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "Received GetNextVotesSyncPacket request";

  const PbftPeriod peer_pbft_period = packet_data.rlp_[0].toInt<PbftPeriod>();
  const PbftRound peer_pbft_round = packet_data.rlp_[1].toInt<PbftRound>();
  const auto [pbft_round, pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  // Send votes only for current_period == peer_period && current_period >= peer_round
  if (pbft_period != peer_pbft_period || pbft_round < peer_pbft_round) {
    LOG(log_nf_) << "No next votes sync packet will be sent. pbft_period " << pbft_period << ", peer_pbft_period "
                 << peer_pbft_period << ", pbft_round " << pbft_round << ", peer_pbft_round " << peer_pbft_round;
    return;
  }

  const auto next_votes_bundle = next_votes_mgr_->getNextVotes();
  std::vector<std::shared_ptr<Vote>> next_votes;
  for (auto &v : next_votes_bundle) {
    if (!peer->isVoteKnown(v->getHash())) {
      next_votes.push_back(std::move(v));
    }
  }

  LOG(log_nf_) << "Next votes sync packet with " << next_votes.size() << " votes sent to " << peer->getId();
  sendPbftVotesBundle(peer, std::move(next_votes));
}

}  // namespace taraxa::network::tarcap
