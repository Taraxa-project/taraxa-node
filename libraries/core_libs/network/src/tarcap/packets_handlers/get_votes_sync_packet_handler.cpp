#include "network/tarcap/packets_handlers/get_votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

GetVotesSyncPacketHandler::GetVotesSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                     std::shared_ptr<PacketsStats> packets_stats,
                                                     std::shared_ptr<PbftManager> pbft_mgr,
                                                     std::shared_ptr<NextVotesManager> next_votes_mgr,
                                                     const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_VOTES_SYNC_PH"),
      pbft_mgr_(std::move(pbft_mgr)),
      next_votes_mgr_(std::move(next_votes_mgr)) {}

void GetVotesSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetVotesSyncPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "Received GetVotesSyncPacket request";

  const uint64_t peer_pbft_round = packet_data.rlp_[0].toPositiveInt64();
  const size_t peer_pbft_previous_round_next_votes_size = packet_data.rlp_[1].toInt<unsigned>();
  const uint64_t pbft_round = pbft_mgr_->getPbftRound();
  const size_t pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();

  if (pbft_round > peer_pbft_round || (pbft_round == peer_pbft_round && pbft_previous_round_next_votes_size >
                                                                            peer_pbft_previous_round_next_votes_size)) {
    LOG(log_dg_) << "Current PBFT round is " << pbft_round << " previous round next votes size "
                 << pbft_previous_round_next_votes_size << ", and peer PBFT round is " << peer_pbft_round
                 << " previous round next votes size " << peer_pbft_previous_round_next_votes_size
                 << ". Will send out bundle of next votes";

    auto next_votes_bundle = next_votes_mgr_->getNextVotes();
    std::vector<std::shared_ptr<Vote>> send_next_votes_bundle;
    for (auto const &v : next_votes_bundle) {
      if (!peer->isVoteKnown(v->getHash())) {
        send_next_votes_bundle.push_back(std::move(v));
      }
    }
    sendPbftNextVotes(packet_data.from_node_id_, send_next_votes_bundle);
  }
}

}  // namespace taraxa::network::tarcap
