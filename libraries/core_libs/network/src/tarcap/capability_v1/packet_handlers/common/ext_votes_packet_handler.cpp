#include "network/tarcap/capability_v1/packets_handlers/common/ext_votes_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote/vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v1 {

ExtVotesPacketHandler::ExtVotesPacketHandler(const FullNodeConfig &conf,
                                             std::shared_ptr<tarcap::PeersState> peers_state,
                                             std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftManager> pbft_mgr,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<VoteManager> vote_mgr, const addr_t &node_addr,
                                             const std::string &log_channel_name)
    : tarcap::ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                                    std::move(pbft_chain), std::move(vote_mgr), node_addr, log_channel_name) {}

void ExtVotesPacketHandler::sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer> &peer,
                                                std::vector<std::shared_ptr<Vote>> &&votes) {
  if (votes.empty()) {
    return;
  }

  size_t index = 0;
  while (index < votes.size()) {
    const size_t count = std::min(static_cast<size_t>(kMaxVotesInBundleRlp), votes.size() - index);
    dev::RLPStream s(count);
    for (auto i = index; i < index + count; i++) {
      const auto &vote = votes[i];
      s.appendRaw(vote->rlp(true, false));
      LOG(log_dg_) << "Send vote " << vote->getHash() << " to peer " << peer->getId();
    }

    if (sealAndSend(peer->getId(), SubprotocolPacketType::VotesBundlePacket, std::move(s))) {
      LOG(log_dg_) << count << " PBFT votes to were sent to " << peer->getId();
      for (auto i = index; i < index + count; i++) {
        peer->markVoteAsKnown(votes[i]->getHash());
      }
    }

    index += count;
  }
}

}  // namespace taraxa::network::tarcap::v1
