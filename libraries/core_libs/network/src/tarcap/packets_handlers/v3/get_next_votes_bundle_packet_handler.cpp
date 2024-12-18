#include "network/tarcap/packets_handlers/v3/get_next_votes_bundle_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v3 {

GetNextVotesBundlePacketHandler::GetNextVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
    std::shared_ptr<SlashingManager> slashing_manager, const addr_t &node_addr, const std::string &logs_prefix)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), std::move(slashing_manager), node_addr,
                            logs_prefix + "GET_NEXT_VOTES_BUNDLE_PH") {}

void GetNextVotesBundlePacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  if (constexpr size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetNextVotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                              const std::shared_ptr<TaraxaPeer> &peer) {
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

  auto next_votes =
      vote_mgr_->getTwoTPlusOneVotedBlockVotes(pbft_period, pbft_round - 1, TwoTPlusOneVotedBlockType::NextVotedBlock);
  auto next_null_votes = vote_mgr_->getTwoTPlusOneVotedBlockVotes(pbft_period, pbft_round - 1,
                                                                  TwoTPlusOneVotedBlockType::NextVotedNullBlock);

  // In edge case this could theoretically happen due to race condition when we moved to the next period or round
  // right before calling getAllTwoTPlusOneNextVotes with specific period & round
  if (next_votes.empty() && next_null_votes.empty()) {
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

    next_votes = vote_mgr_->getTwoTPlusOneVotedBlockVotes(pbft_period, pbft_round - 1,
                                                          TwoTPlusOneVotedBlockType::NextVotedBlock);
    next_null_votes = vote_mgr_->getTwoTPlusOneVotedBlockVotes(pbft_period, pbft_round - 1,
                                                               TwoTPlusOneVotedBlockType::NextVotedNullBlock);
    if (next_votes.empty() && next_null_votes.empty()) {
      LOG(log_er_) << "No next votes returned for period " << tmp_pbft_period << ", round " << tmp_pbft_round - 1;
      return;
    }
  }

  if (!next_votes.empty()) {
    LOG(log_nf_) << "Send next votes bundle with " << next_votes.size() << " votes to " << peer->getId();
    sendPbftVotesBundle(peer, std::move(next_votes));
  }

  if (!next_null_votes.empty()) {
    LOG(log_nf_) << "Send next null votes bundle with " << next_null_votes.size() << " votes to " << peer->getId();
    sendPbftVotesBundle(peer, std::move(next_null_votes));
  }
}

}  // namespace taraxa::network::tarcap::v3
