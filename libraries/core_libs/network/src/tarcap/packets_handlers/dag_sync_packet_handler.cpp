#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "votes/rewards_votes.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<SyncingState> syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr,
                                           std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                                           std::shared_ptr<RewardsVotes> rewards_votes, const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)),
      rewards_votes_(std::move(rewards_votes)) {}

void DagSyncPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  std::string received_dag_blocks_str;
  std::unordered_set<blk_hash_t> missing_blks;

  for (auto it = packet_data.rlp_.begin(); it != packet_data.rlp_.end(); it++) {
    const auto& block_data_rlp = *it;
    if (block_data_rlp.itemCount() != 3) {
      LOG(log_dg_) << "Received invalid DagSyncBlockPacket from node " << peer->getId()
                   << ", Reason: ItemsCount=" << packet_data.rlp_.itemCount();
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    DagBlock block(block_data_rlp[0]);
    peer->markDagBlockAsKnown(block.getHash());

    // TODO: this could save a lot of processing - need to check if we can do that
    // Do not process this block in case we already have it
    //    if (dag_blk_mgr_->isDagBlockKnown(block.getHash())) {
    //      LOG(log_dg_) << "Ignore new dag block " << hash.abridged() << ", block is already known";
    //      continue;
    //    }

    // Parses txs
    const auto txs_rlp = block_data_rlp[1];
    SharedTransactions txs;
    txs.reserve(txs_rlp.itemCount());
    for (auto txs_it = txs_rlp.begin(); txs_it != txs_rlp.end(); txs_it++) {
      const auto& inserted_tx = txs.emplace_back(std::make_shared<Transaction>(*txs_it));
      peer->markTransactionAsKnown(inserted_tx->getHash());
    }

    // Parses votes
    const auto& votes_rlps = block_data_rlp[2];
    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> votes;
    votes.reserve(votes_rlps.itemCount());
    for (const auto& vote_rlp : votes_rlps) {
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

    received_dag_blocks_str += block.getHash().abridged() + " ";

    auto status = checkDagBlockValidation(block);
    if (!status.first) {
      LOG(log_er_) << "DagBlock" << block.getHash() << " Validation failed " << status.second;
      status.second.insert(block.getHash());
      missing_blks.merge(status.second);
      continue;
    }

    LOG(log_dg_) << "Storing block " << block.getHash().abridged() << " with " << txs.size() << " txs and "
                 << unknown_votes.size() << " votes";
    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();

    // Inserts votes into the rewards_votes_
    rewards_votes_->insertVotes(std::move(unknown_votes));

    trx_mgr_->insertBroadcastedTransactions(std::move(txs));
    dag_blk_mgr_->insertBroadcastedBlock(std::move(block));
  }

  if (missing_blks.size() > 0) {
    requestDagBlocks(packet_data.from_node_id_, missing_blks, DagSyncRequestType::MissingHashes);
  }
  syncing_state_->set_dag_syncing(false);

  LOG(log_nf_) << "Received DagDagSyncPacket with blocks: " << received_dag_blocks_str;
}

}  // namespace taraxa::network::tarcap
