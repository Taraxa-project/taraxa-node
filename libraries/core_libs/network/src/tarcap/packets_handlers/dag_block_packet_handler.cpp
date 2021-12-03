#include "network/tarcap/packets_handlers/dag_block_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "network/tarcap/shared_states/test_state.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "votes/rewards_votes.hpp"

namespace taraxa::network::tarcap {

DagBlockPacketHandler::DagBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<PacketsStats> packets_stats,
                                             std::shared_ptr<SyncingState> syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<DagBlockManager> dag_blk_mgr,
                                             std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                                             std::shared_ptr<TestState> test_state,
                                             std::shared_ptr<RewardsVotes> rewards_votes, const addr_t &node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "DAG_BLOCK_PH"),
      test_state_(std::move(test_state)),
      trx_mgr_(std::move(trx_mgr)),
      rewards_votes_(std::move(rewards_votes)) {}

thread_local std::mt19937_64 DagBlockPacketHandler::urng_{std::mt19937_64(std::random_device()())};

void DagBlockPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  if (packet_data.rlp_.itemCount() != 2) {
    LOG(log_dg_) << "Received invalid DagBlockPacket from node " << peer->getId()
                 << ", Reason: ItemsCount=" << packet_data.rlp_.itemCount();
    disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    return;
  }

  DagBlock block(packet_data.rlp_[0].data().toBytes());
  blk_hash_t const hash = block.getHash();
  LOG(log_dg_) << "Received DagBlockPacket " << hash.abridged() << "from: " << peer->getId().abridged();

  peer->markDagBlockAsKnown(hash);

  if (dag_blk_mgr_) {
    // Do not process this block in case we already have it
    if (dag_blk_mgr_->isDagBlockKnown(block.getHash())) {
      LOG(log_dg_) << "Ignore new dag block " << hash.abridged() << ", block is already known";
      return;
    }

    // Parses votes
    const auto &votes_rlps = packet_data.rlp_[1];
    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> votes;
    votes.reserve(votes_rlps.itemCount());
    for (const auto &vote_rlp : votes_rlps) {
      auto vote = std::make_shared<Vote>(vote_rlp);
      auto vote_hash = vote->getHash();
      peer->markVoteAsKnown(vote_hash);

      if (!votes.emplace(std::move(vote_hash), std::move(vote)).second) {
        LOG(log_dg_) << "Received invalid DagBlockPacket from node " << peer->getId()
                     << ", Reason: Duplicate vote included: " << vote_hash.abridged();
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    // Filters out new (unknown) votes
    auto unknown_votes = rewards_votes_->filterUnknownVotes(votes);

    // Validates block
    // TODO: this validation should replace verifyBlock & unverified_queue becomes useless after that
    if (auto res = dag_blk_mgr_->validateBlock(block, unknown_votes); !res.first) {
      LOG(log_dg_) << "Received invalid DagBlock from node " << peer->getId() << ", Reason: : " << res.second;
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // TODO: This function should probably check only missing tips/pivot.
    //       There is a big difference between failing a validation due to missing tips/pivot and
    //       invalid signature/data/etc... In case of missing tips/pivot we can request those, otherwise we throw
    //       away such block
    if (auto status = checkDagBlockValidation(block); !status.first) {
      // Ignore new block packets when pbft syncing
      if (syncing_state_->is_pbft_syncing()) {
        LOG(log_dg_) << "Ignore new dag block " << hash.abridged() << ", pbft syncing is on";
        return;
      }

      LOG(log_er_) << "Received NewBlock " << hash << " has missing pivot or/and tips " << status.second;
      status.second.insert(hash);
      requestDagBlocks(packet_data.from_node_id_, status.second, DagSyncRequestType::MissingHashes);
      return;
    }

    // Inserts votes into the rewards_votes_
    rewards_votes_->insertVotes(std::move(unknown_votes));
  }

  if (block.getLevel() > peer->dag_level_) {
    peer->dag_level_ = block.getLevel();
  }

  onNewBlockReceived(std::move(block));
}

void DagBlockPacketHandler::sendBlock(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block) {
  std::shared_ptr<TaraxaPeer> peer = peers_state_->getPeer(peer_id);
  if (!peer) {
    LOG(log_wr_) << "Send dag block " << block.getHash() << ". Failed to obtain peer " << peer_id.abridged();
    return;
  }

  vec_trx_t transactions_to_send;
  for (const auto &trx_hash : block.getTrxs()) {
    if (peer->isTransactionKnown(trx_hash)) {
      continue;
    }
    transactions_to_send.push_back(trx_hash);
  }

  // Transactions are first sent in transactions packet before sending the block
  dev::RLPStream txs_stream(transactions_to_send.size());
  taraxa::bytes trx_bytes;
  std::shared_ptr<Transaction> transaction;
  for (const auto &trx_hash : transactions_to_send) {
    if (dag_blk_mgr_) {
      transaction = trx_mgr_->getTransaction(trx_hash);
      assert(transaction != nullptr);  // We should never try to send a block for
                                       // which  we do not have all transactions
    } else {
      assert(test_state_->hasTransaction(trx_hash));
      transaction = std::make_shared<Transaction>(test_state_->getTransaction(trx_hash));
    }
    auto &trx_data = *transaction->rlp();
    trx_bytes.insert(trx_bytes.end(), std::begin(trx_data), std::end(trx_data));
  }
  txs_stream.appendRaw(trx_bytes, transactions_to_send.size());

  // Try to send data over network
  if (!sealAndSend(peer_id, TransactionPacket, std::move(txs_stream))) {
    LOG(log_wr_) << "Sending sync TransactionPacket for dag block: " << block.getHash()
                 << " failed. Block was not sent either.";
    return;
  }

  // Mark data as known if sending was successful
  for (const auto &trx_hash : transactions_to_send) {
    peer->markTransactionAsKnown(trx_hash);
  }

  // Send the block
  dev::RLPStream block_stream(2);
  block_stream.appendRaw(block.rlp(true));

  // Append also unknown rewards votes. Votes have to be sent together with dag block(not in separate packet)
  // because they are validated based on mapped pbft period from dag block level, in which they are included
  auto peer_unknown_votes_hashes = peer->filterUnknownVotes(block.getRewardsVotes());
  size_t peer_unknown_votes_count = peer_unknown_votes_hashes.size();

  auto peer_unknown_votes = rewards_votes_->getVotes(std::move(peer_unknown_votes_hashes));
  // Not all votes were found
  if (!peer_unknown_votes.first) {
    // This should not happen - if some block was accepted, all rewards votes should have been saved already
    std::string missing_votes{""};
    // Not all votes were found - rewards_votes_->getVotes returned missing votes instead of found ones
    for (const auto &missing_vote : peer_unknown_votes.second) {
      missing_votes += missing_vote.first.abridged() + ", ";
    }

    // Should not be happening - keep ERROR
    LOG(log_er_) << "Cannot send DagBlock(" << block.getHash()
                 << ") due to incomplete data (missing rewards votes): " << missing_votes;
    return;
  }

  block_stream.appendList(peer_unknown_votes_count);
  for (const auto &unknown_vote : peer_unknown_votes.second) {
    block_stream.appendRaw(unknown_vote.second->rlp(true));
  }

  // Try to send data over network
  if (!sealAndSend(peer_id, DagBlockPacket, std::move(block_stream))) {
    LOG(log_wr_) << "Sending DagBlock " << block.getHash() << " failed";
    return;
  }

  // Mark data as known if sending was successful
  peer->markDagBlockAsKnown(block.getHash());
  for (const auto &vote : peer_unknown_votes.second) {
    peer->markVoteAsKnown(vote.first);
  }

  LOG(log_dg_) << "Send DagBlock " << block.getHash() << " #Trx: " << transactions_to_send.size();
}

void DagBlockPacketHandler::onNewBlockReceived(DagBlock &&block) {
  LOG(log_nf_) << "Receive DagBlock " << block.getHash();
  if (dag_blk_mgr_) {
    LOG(log_nf_) << "Storing block " << block.getHash().toString();
    dag_blk_mgr_->insertBroadcastedBlock(block);

  } else if (!test_state_->hasBlock(block.getHash())) {
    test_state_->insertBlock(block);
    onNewBlockVerified(block, false);

  } else {
    LOG(log_dg_) << "Received NewBlock " << block.getHash() << "that is already known";
    return;
  }
}

void DagBlockPacketHandler::onNewBlockVerified(DagBlock const &block, bool proposed) {
  // If node is pbft syncing and block is not proposed by us, this is an old block that has been verified - no block
  // goosip is needed
  if (!proposed && syncing_state_->is_deep_pbft_syncing()) {
    return;
  }

  const auto &block_hash = block.getHash();
  LOG(log_dg_) << "Verified NewBlock " << block_hash.toString();

  std::vector<dev::p2p::NodeID> peers_to_send;
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isDagBlockKnown(block_hash) && !peer.second->syncing_) {
      peers_to_send.push_back(peer.first);
    }
  }

  for (dev::p2p::NodeID const &peer_id : peers_to_send) {
    dev::RLPStream ts;
    auto peer = peers_state_->getPeer(peer_id);
    if (peer && !peer->syncing_) {
      sendBlock(peer_id, block);
      peer->markDagBlockAsKnown(block_hash);
    }
  }
  if (!peers_to_send.empty()) LOG(log_dg_) << "Sent block to " << peers_to_send.size() << " peers";
}
}  // namespace taraxa::network::tarcap
