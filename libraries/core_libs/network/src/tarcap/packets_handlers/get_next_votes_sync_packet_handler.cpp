#include "network/tarcap/packets_handlers/get_next_votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

GetNextVotesSyncPacketHandler::GetNextVotesSyncPacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), node_addr, "GET_NEXT_VOTES_SYNC_PH") {}

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
  if (pbft_period != peer_pbft_period || pbft_round == 1 || pbft_round < peer_pbft_round) {
    LOG(log_nf_) << "No previous round next votes sync packet will be sent. pbft_period " << pbft_period
                 << ", peer_pbft_period " << peer_pbft_period << ", pbft_round " << pbft_round << ", peer_pbft_round "
                 << peer_pbft_round;
    return;
  }

  std::vector<std::shared_ptr<Vote>> next_votes = vote_mgr_->getAllTwoTPlusOneNextVotes(pbft_period, pbft_round - 1);
  // In edge case this could theoretically happen due to race condition when we moved to the next period or round
  // right before calling getAllTwoTPlusOneNextVotes with specific period & round
  if (next_votes.empty()) {
    // Try to get period & round values again
    const auto [tmp_pbft_round, tmp_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();
    // No changes in period & round or new round == 1
    if (pbft_period == tmp_pbft_period && pbft_round == tmp_pbft_round) {
      LOG(log_er_) << "No next votes returned for period " << tmp_pbft_period << ", round " << tmp_pbft_round - 1;
      return;
    }

    if (tmp_pbft_round == 1) {
      LOG(log_wr_) << "No next votes returned for period " << tmp_pbft_period << ", round " << tmp_pbft_round - 1
                   << " due to race condition - pbft already moved to the next period & round == 1";
      return;
    }

    next_votes = vote_mgr_->getAllTwoTPlusOneNextVotes(tmp_pbft_period, tmp_pbft_round - 1);
    if (next_votes.empty()) {
      LOG(log_er_) << "No next votes returned for period " << tmp_pbft_period << ", round " << tmp_pbft_round - 1;
      return;
    }
  }

  std::vector<std::shared_ptr<Vote>> next_votes_to_send;
  next_votes_to_send.reserve(next_votes.size());
  for (const auto &vote : next_votes) {
    if (!peer->isVoteKnown(vote->getHash())) {
      next_votes_to_send.emplace_back(vote);
    }
  }

  if (next_votes_to_send.empty()) {
    LOG(log_dg_) << "Votes already gossiped, no need to send votes sync packet for" << pbft_period << ", round "
                 << pbft_round - 1;
    return;
  }

  LOG(log_nf_) << "Next votes sync packet with " << next_votes.size() << " votes sent to " << peer->getId();
  sendPbftVotesBundle(peer, std::move(next_votes));
}

}  // namespace taraxa::network::tarcap
