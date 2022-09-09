#include "network/tarcap/packets_handlers/propose_block_and_vote_packet_handler.hpp"

#include <libp2p/Common.h>

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

ProposeBlockAndVotePacketHandler::ProposeBlockAndVotePacketHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<PacketsStats> packets_stats,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<VoteManager> vote_mgr,
                                               const NetworkConfig& net_config, const addr_t &node_addr)
    : ExtVotesPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_mgr),
                            std::move(pbft_chain), std::move(vote_mgr), net_config, node_addr, "PROPOSE_BLOCK_VOTE_PH")
      {}

void ProposeBlockAndVotePacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 3; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void ProposeBlockAndVotePacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_tr_) << "In ProposeBlockAndVotePacket";

  auto proposed_block = std::make_shared<PbftBlock>(packet_data.rlp_[0]);
  auto propose_vote = std::make_shared<Vote>(packet_data.rlp_[1].data().toBytes());
  const uint64_t peer_pbft_chain_size = packet_data.rlp_[3].toInt();

  // Update peer's max chain size
  if (peer_pbft_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = peer_pbft_chain_size;
  }

  LOG(log_dg_) << "Received proposed PBFT Block " << proposed_block->getBlockHash().abridged() << " with propose vote " << propose_vote->getHash()
               << ", Peer PBFT Chain size: " << peer_pbft_chain_size << " from " << peer->getId();

  if (!validateProposeBlockAndVote(proposed_block, propose_vote)) {
    LOG(log_er_) << "Invalid data in ProposeBlockAndVotePacket. Set peer " << packet_data.from_node_id_.abridged() << " as malicious";
    handleMaliciousPeer(packet_data.from_node_id_);
    return;
  }

  // TODO: should we somehow validate also proposed block ???
//  if (proposed_block->getPrevBlockHash() != getLastPbftBlockHash()) {
//    if (findPbftBlockInChain(proposed_block->getBlockHash())) {
//      // The block comes from slow node, drop
//      LOG(log_dg_) << "Cannot add the PBFT block " << proposed_block->getBlockHash() << " since it's in chain already";
//      return false;
//    } else {
//      // TODO: The block comes from fast node that should insert.
//      //  Or comes from malicious node, need check
//    }
//  }

  peer->markPbftBlockAsKnown(proposed_block->getBlockHash());
  const auto pbft_synced_period = pbft_mgr_->pbftSyncingPeriod();
  if (pbft_synced_period >= proposed_block->getPeriod()) {
    LOG(log_dg_) << "Drop proposed PBFT block " << proposed_block->getBlockHash().abridged() << " for period "
                 << proposed_block->getPeriod() << ", own PBFT chain has synced at period " << pbft_synced_period;
    return;
  }

  // Reuse votes protection also for proposed blocks -> 1 account can create only 1 unique vote per period & round & step
  // This way we can also limit number of unique proposed blocks -> insert block only if vote was inserted
  if (processStandardVote(propose_vote, peer, true)) {
    LOG(log_dg_) << "Unable to process propose vote " << propose_vote->getHash();
    return;
  }

  // Push proposed pbft block only after propose vote was successfully pushed
  if (pbft_mgr_->pushProposedBlock(proposed_block, propose_vote)) {
    LOG(log_er_) << "Proposed block " << proposed_block->getBlockHash() << " push failed";
    return;
  }

  onNewProposeBlockAndVote(proposed_block, propose_vote);
}

bool ProposeBlockAndVotePacketHandler::validateProposeBlockAndVote(const std::shared_ptr<PbftBlock> &proposed_block, const std::shared_ptr<Vote> &propose_vote) {
  if (proposed_block->getBlockHash() != propose_vote->getBlockHash()) {
    LOG(log_er_) << "Propose vote " << propose_vote->getHash() << " voted block " << propose_vote->getBlockHash()
                 << " != proposed block " << proposed_block->getBlockHash();
    return false;
  }

  if (propose_vote->getType() != PbftVoteTypes::propose_vote_type) {
    LOG(log_er_) << "Propose vote " << propose_vote->getHash() << " type " << static_cast<int>(propose_vote->getType())
                 << " != propose_type " << static_cast<int>(PbftVoteTypes::propose_vote_type);
    return false;
  }

  if (propose_vote->getStep() != PbftStates::value_proposal_state) {
    LOG(log_er_) << "Propose vote " << propose_vote->getHash() << " step " << propose_vote->getStep()
                 << " != proposal_step " << static_cast<int>(PbftStates::value_proposal_state);
    return false;
  }

  return true;
}

void ProposeBlockAndVotePacketHandler::onNewProposeBlockAndVote(const std::shared_ptr<PbftBlock> &proposed_block, const std::shared_ptr<Vote> &propose_vote) {
  if (!validateProposeBlockAndVote(proposed_block, propose_vote)) {
    // This should never happen
    LOG(log_er_) << "Invalid data in onNewProposeBlockAndVote";
    assert(false);
    return;
  }

  std::vector<std::shared_ptr<TaraxaPeer>> peers_to_send;
  const auto my_chain_size = pbft_chain_->getPbftChainSize();

  const auto pbft_block_hash = proposed_block->getBlockHash();
  const auto dag_anchor_hash = proposed_block->getPivotDagBlockHash();
  std::string peers_to_log;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isPbftBlockKnown(pbft_block_hash) && !peer.second->syncing_) {
      if (!peer.second->isDagBlockKnown(dag_anchor_hash)) {
        LOG(log_tr_) << "sending PbftBlock " << pbft_block_hash << " with missing dag anchor " << dag_anchor_hash
                     << " to " << peer.first;
      }
      peers_to_send.emplace_back(peer.second);
      peers_to_log += peer.second->getId().abridged();
    }
  }

  vote_mgr_->sendRewardVotes(proposed_block->getPrevBlockHash());

  LOG(log_dg_) << "sendPbftBlock " << pbft_block_hash << " to " << peers_to_log;
  for (auto const &peer : peers_to_send) {
    sendProposeBlockAndVote(peer->getId(), proposed_block, propose_vote, my_chain_size);
    peer->markPbftBlockAsKnown(pbft_block_hash);
  }
}

void ProposeBlockAndVotePacketHandler::sendProposeBlockAndVote(const dev::p2p::NodeID &peer_id,
                                           const std::shared_ptr<PbftBlock> &pbft_block, const std::shared_ptr<Vote> &propose_vote,
                                                               uint64_t pbft_chain_size) {
  LOG(log_tr_) << "Send ProposeBlockAndVote packet. Block " << pbft_block->getBlockHash() << ", vote " << propose_vote->getHash() << " to " << peer_id;
  dev::RLPStream s(3);
  pbft_block->streamRLP(s, true);
  s << propose_vote->rlp(true);
  s << pbft_chain_size;
  sealAndSend(peer_id, ProposeBlockAndVotePacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
