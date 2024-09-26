#include "network/tarcap/packets_handlers/latest/vote_packet_handler.hpp"

#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

VotePacketHandler::VotePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                     std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                     std::shared_ptr<VoteManager> vote_mgr,
                                     std::shared_ptr<SlashingManager> slashing_manager, const addr_t &node_addr,
                                     const std::string &logs_prefix)
    : ExtVotesPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), std::move(slashing_manager), node_addr,
                            logs_prefix + "PBFT_VOTE_PH") {}

void VotePacketHandler::process(VotePacket &&packet, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto [current_pbft_round, current_pbft_period] = pbft_mgr_->getPbftRoundAndPeriod();

  if (packet.pbft_block) {
    LOG(log_dg_) << "Received PBFT vote " << packet.vote->getHash() << " with PBFT block "
                 << packet.pbft_block->getBlockHash();
  } else {
    LOG(log_dg_) << "Received PBFT vote " << packet.vote->getHash();
  }

  // Update peer's max chain size
  if (packet.peer_chain_size.has_value() && *packet.peer_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = *packet.peer_chain_size;
  }

  const auto vote_hash = packet.vote->getHash();

  if (!isPbftRelevantVote(packet.vote)) {
    LOG(log_dg_) << "Drop irrelevant vote " << vote_hash << " for current pbft state. Vote (period, round, step) = ("
                 << packet.vote->getPeriod() << ", " << packet.vote->getRound() << ", " << packet.vote->getStep()
                 << "). Current PBFT (period, round, step) = (" << current_pbft_period << ", " << current_pbft_round
                 << ", " << pbft_mgr_->getPbftStep() << ")";
    return;
  }

  // Do not process vote that has already been validated
  if (vote_mgr_->voteAlreadyValidated(vote_hash)) {
    LOG(log_dg_) << "Received vote " << vote_hash << " has already been validated";
    return;
  }

  if (packet.pbft_block) {
    if (packet.pbft_block->getBlockHash() != packet.vote->getBlockHash()) {
      std::ostringstream err_msg;
      err_msg << "Vote " << packet.vote->getHash().abridged() << " voted block "
              << packet.vote->getBlockHash().abridged() << " != actual block "
              << packet.pbft_block->getBlockHash().abridged();
      throw MaliciousPeerException(err_msg.str());
    }

    peer->markPbftBlockAsKnown(packet.pbft_block->getBlockHash());
  }

  if (!processVote(packet.vote, packet.pbft_block, peer, true)) {
    return;
  }

  // Do not mark it before, as peers have small caches of known votes. Only mark gossiping votes
  peer->markPbftVoteAsKnown(vote_hash);

  pbft_mgr_->gossipVote(packet.vote, packet.pbft_block);
}

void VotePacketHandler::onNewPbftVote(const std::shared_ptr<PbftVote> &vote, const std::shared_ptr<PbftBlock> &block,
                                      bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " not sent to " << peer.first << " peer syncing";
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

void VotePacketHandler::sendPbftVote(const std::shared_ptr<TaraxaPeer> &peer, const std::shared_ptr<PbftVote> &vote,
                                     const std::shared_ptr<PbftBlock> &block) {
  if (block && block->getBlockHash() != vote->getBlockHash()) {
    LOG(log_er_) << "Vote " << vote->getHash().abridged() << " voted block " << vote->getBlockHash().abridged()
                 << " != actual block " << block->getBlockHash().abridged();
    return;
  }

  dev::RLPStream s;

  if (block) {
    s = dev::RLPStream(kExtendedVotePacketSize);
    s.appendRaw(vote->rlp(true, false));
    s.appendRaw(block->rlp(true));
    s.append(pbft_chain_->getPbftChainSize());
  } else {
    s = dev::RLPStream(kVotePacketSize);
    s.appendRaw(vote->rlp(true, false));
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::kVotePacket, std::move(s))) {
    peer->markPbftVoteAsKnown(vote->getHash());
    if (block) {
      peer->markPbftBlockAsKnown(block->getBlockHash());
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " together with block " << block->getBlockHash()
                   << " sent to " << peer->getId();
    } else {
      LOG(log_dg_) << " PBFT vote " << vote->getHash() << " sent to " << peer->getId();
    }
  }
}

}  // namespace taraxa::network::tarcap
