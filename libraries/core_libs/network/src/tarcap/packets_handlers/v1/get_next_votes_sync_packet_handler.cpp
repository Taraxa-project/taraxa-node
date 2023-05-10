#include "network/tarcap/packets_handlers/v1/get_next_votes_sync_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v1 {

void GetNextVotesBundlePacketHandler::sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer> &peer,
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
