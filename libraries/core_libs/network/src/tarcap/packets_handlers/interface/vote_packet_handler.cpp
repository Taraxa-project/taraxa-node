#include "network/tarcap/packets_handlers/interface/vote_packet_handler.hpp"

#include "network/tarcap/packets/latest/vote_packet.hpp"

namespace taraxa::network::tarcap {

IVotePacketHandler::IVotePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                       std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                       std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                       std::shared_ptr<VoteManager> vote_mgr,
                                       std::shared_ptr<SlashingManager> slashing_manager,
                                       const std::string &log_channel_name)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), std::move(slashing_manager), log_channel_name) {
}

void IVotePacketHandler::onNewPbftVote(const std::shared_ptr<PbftVote> &vote, const std::shared_ptr<PbftBlock> &block,
                                       bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      logger_->debug(" PBFT vote {} not sent to {} peer syncing", vote->getHash(), peer.first);
      continue;
    }

    if (!rebroadcast && peer.second->isPbftVoteKnown(vote->getHash())) {
      continue;
    }

    // Send also block in case it is not known for the pear or rebroadcast == true
    if (rebroadcast || !peer.second->isPbftBlockKnown(vote->getBlockHash())) {
      sendPbftVote(peer.second, vote, block);
    } else {
      sendPbftVote(peer.second, vote, nullptr);
    }
  }
}

void IVotePacketHandler::onNewPbftVotesBundle(const std::vector<std::shared_ptr<PbftVote>> &votes, bool rebroadcast,
                                              const std::optional<dev::p2p::NodeID> &exclude_node) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      continue;
    }

    if (exclude_node.has_value() && *exclude_node == peer.first) {
      continue;
    }

    std::vector<std::shared_ptr<PbftVote>> peer_votes;
    for (const auto &vote : votes) {
      if (!rebroadcast && peer.second->isPbftVoteKnown(vote->getHash())) {
        continue;
      }

      peer_votes.push_back(vote);
    }

    sendPbftVotesBundle(peer.second, std::move(peer_votes));
  }
}

void IVotePacketHandler::sendPbftVote(const std::shared_ptr<TaraxaPeer> &peer, const std::shared_ptr<PbftVote> &vote,
                                      const std::shared_ptr<PbftBlock> &block) {
  if (block && block->getBlockHash() != vote->getBlockHash()) {
    logger_->error("Vote {} voted block {} != actual block {}", vote->getHash().abridged(),
                   vote->getBlockHash().abridged(), block->getBlockHash().abridged());
    return;
  }

  std::optional<VotePacket::OptionalData> optional_packet_data;
  if (block) {
    optional_packet_data = VotePacket::OptionalData{block, pbft_chain_->getPbftChainSize()};
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::kVotePacket,
                  encodePacketRlp(VotePacket(vote, std::move(optional_packet_data))))) {
    peer->markPbftVoteAsKnown(vote->getHash());
    if (block) {
      peer->markPbftBlockAsKnown(block->getBlockHash());
      logger_->debug(" PBFT vote {} together with block {} sent to ", vote->getHash(), block->getBlockHash(),
                     peer->getId());
    } else {
      logger_->debug(" PBFT vote {} sent to {}", vote->getHash(), peer->getId());
    }
  }
}

void IVotePacketHandler::sendPbftVotesBundle(const std::shared_ptr<TaraxaPeer> &peer,
                                             std::vector<std::shared_ptr<PbftVote>> &&votes) {
  if (votes.empty()) {
    return;
  }

  auto sendVotes = [this, &peer](std::vector<std::shared_ptr<PbftVote>> &&votes) {
    auto packet = VotesBundlePacket{OptimizedPbftVotesBundle{.votes = std::move(votes)}};
    if (this->sealAndSend(peer->getId(), SubprotocolPacketType::kVotesBundlePacket, encodePacketRlp(packet))) {
      logger_->debug(" Votes bundle with {} votes sent to {}", packet.votes_bundle.votes.size(), peer->getId());
      for (const auto &vote : packet.votes_bundle.votes) {
        peer->markPbftVoteAsKnown(vote->getHash());
      }
    }
  };

  if (votes.size() <= kMaxVotesInBundleRlp) {
    sendVotes(std::move(votes));
    return;
  } else {
    // Need to split votes into multiple packets
    size_t index = 0;
    while (index < votes.size()) {
      const size_t votes_count = std::min(kMaxVotesInBundleRlp, votes.size() - index);

      const auto begin_it = std::next(votes.begin(), index);
      const auto end_it = std::next(begin_it, votes_count);

      std::vector<std::shared_ptr<PbftVote>> votes_sub_vector;
      std::move(begin_it, end_it, std::back_inserter(votes_sub_vector));

      sendVotes(std::move(votes_sub_vector));

      index += votes_count;
    }
  }
}

}  // namespace taraxa::network::tarcap
