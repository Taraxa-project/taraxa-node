#include "network/tarcap/packets_handlers/latest/pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote/vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

PbftSyncPacketHandler::PbftSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                                             const addr_t &node_addr, const std::string &log_channel_name)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              log_channel_name),
      vote_mgr_(std::move(vote_mgr)),
      periodic_events_tp_(1, true) {}

void PbftSyncPacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  if (packet_data.rlp_.itemCount() != kStandardPacketSize && packet_data.rlp_.itemCount() != kChainSyncedPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), kStandardPacketSize);
  }

  // PeriodData rlp parsing cannot be done through util::rlp_tuple, which automatically checks the rlp size so it is
  // checked here manually
  if (packet_data.rlp_[1].itemCount() != PeriodData::kRlpItemCount) {
    throw InvalidRlpItemsCountException(packet_data.type_str_ + ":PeriodData", packet_data.rlp_[1].itemCount(),
                                        PeriodData::kRlpItemCount);
  }
}

void PbftSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                    const std::shared_ptr<TaraxaPeer> &peer) {
  // Note: no need to consider possible race conditions due to concurrent processing as it is
  // disabled on priority_queue blocking dependencies level
  const auto syncing_peer = pbft_syncing_state_->syncingPeer();
  if (!syncing_peer) {
    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
                 << " but there is no current syncing peer set";
    return;
  }

  if (syncing_peer->getId() != packet_data.from_node_id_) {
    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
                 << " current syncing peer " << syncing_peer->getId().abridged();
    return;
  }

  // Process received pbft blocks
  // pbft_chain_synced is the flag to indicate own PBFT chain has synced with the peer's PBFT chain
  const bool pbft_chain_synced = packet_data.rlp_.itemCount() == kChainSyncedPacketSize;
  // last_block is the flag to indicate this is the last block in each syncing round, doesn't mean PBFT chain has synced
  const bool last_block = packet_data.rlp_[0].toInt<bool>();
  PeriodData period_data;
  try {
    period_data = decodePeriodData(packet_data.rlp_[1]);
  } catch (const std::runtime_error &e) {
    throw MaliciousPeerException("Unable to parse PeriodData: " + std::string(e.what()));
  }

  std::vector<std::shared_ptr<Vote>> current_block_cert_votes;
  if (pbft_chain_synced) {
    current_block_cert_votes = decodeVotesBundle(packet_data.rlp_[2]);
  }
  const auto pbft_blk_hash = period_data.pbft_blk->getBlockHash();

  std::string received_dag_blocks_str;  // This is just log related stuff
  for (auto const &block : period_data.dag_blocks) {
    received_dag_blocks_str += block.getHash().toString() + " ";
    if (peer->dag_level_ < block.getLevel()) {
      peer->dag_level_ = block.getLevel();
    }
  }
  const auto pbft_block_period = period_data.pbft_blk->getPeriod();

  LOG(log_dg_) << "PbftSyncPacket received. Period: " << pbft_block_period
               << ", dag Blocks: " << received_dag_blocks_str << " from " << packet_data.from_node_id_;

  peer->markPbftBlockAsKnown(pbft_blk_hash);
  // Update peer's pbft period if outdated
  if (peer->pbft_chain_size_ < pbft_block_period) {
    peer->pbft_chain_size_ = pbft_block_period;
  }

  LOG(log_tr_) << "Processing pbft block: " << pbft_blk_hash;

  if (pbft_chain_->findPbftBlockInChain(pbft_blk_hash)) {
    LOG(log_wr_) << "PBFT block " << pbft_blk_hash << ", period: " << period_data.pbft_blk->getPeriod() << " from "
                 << packet_data.from_node_id_ << " already present in chain";
  } else {
    if (pbft_block_period != pbft_mgr_->pbftSyncingPeriod() + 1) {
      LOG(log_er_) << "Block " << pbft_blk_hash << " period unexpected: " << pbft_block_period
                   << ". Expected period: " << pbft_mgr_->pbftSyncingPeriod() + 1;
      return;
    }

    // Check cert vote matches if final synced block
    if (pbft_chain_synced) {
      for (auto const &vote : current_block_cert_votes) {
        if (vote->getBlockHash() != pbft_blk_hash) {
          LOG(log_er_) << "Invalid cert votes block hash " << vote->getBlockHash() << " instead of " << pbft_blk_hash
                       << " from peer " << packet_data.from_node_id_.abridged() << " received, stop syncing.";
          handleMaliciousSyncPeer(packet_data.from_node_id_);
          return;
        }
      }
    }

    // Check votes match the hash of previous block in the queue
    auto last_pbft_block_hash = pbft_mgr_->lastPbftBlockHashFromQueueOrChain();
    // Check cert vote matches
    for (auto const &vote : period_data.previous_block_cert_votes) {
      if (vote->getBlockHash() != last_pbft_block_hash) {
        LOG(log_er_) << "Invalid cert votes block hash " << vote->getBlockHash() << " instead of "
                     << last_pbft_block_hash << " from peer " << packet_data.from_node_id_.abridged()
                     << " received, stop syncing.";
        handleMaliciousSyncPeer(packet_data.from_node_id_);
        return;
      }
    }

    auto order_hash = PbftManager::calculateOrderHash(period_data.dag_blocks);
    if (order_hash != period_data.pbft_blk->getOrderHash()) {
      {  // This is just log related stuff
        std::vector<trx_hash_t> trx_order;
        trx_order.reserve(period_data.transactions.size());
        std::vector<blk_hash_t> blk_order;
        blk_order.reserve(period_data.dag_blocks.size());
        for (auto t : period_data.transactions) {
          trx_order.push_back(t->getHash());
        }
        for (auto b : period_data.dag_blocks) {
          blk_order.push_back(b.getHash());
        }
        LOG(log_er_) << "Order hash incorrect in period data " << pbft_blk_hash << " expected: " << order_hash
                     << " received " << period_data.pbft_blk->getOrderHash() << "; Dag order: " << blk_order
                     << "; Trx order: " << trx_order << "; from " << packet_data.from_node_id_.abridged()
                     << ", stop syncing.";
      }
      handleMaliciousSyncPeer(packet_data.from_node_id_);
      return;
    }

    // This is special case when queue is empty and we can not say for sure that all votes that are part of this block
    // have been verified before
    if (pbft_mgr_->periodDataQueueEmpty()) {
      for (const auto &v : period_data.previous_block_cert_votes) {
        if (auto vote_is_valid = vote_mgr_->validateVote(v); vote_is_valid.first == false) {
          LOG(log_er_) << "Invalid reward votes in block " << period_data.pbft_blk->getBlockHash() << " from peer "
                       << packet_data.from_node_id_.abridged()
                       << " received, stop syncing. Validation failed. Err: " << vote_is_valid.second;
          handleMaliciousSyncPeer(packet_data.from_node_id_);
          return;
        }

        vote_mgr_->addVerifiedVote(v);
      }

      // And now we need to replace it with verified votes
      if (auto votes = vote_mgr_->checkRewardVotes(period_data.pbft_blk, true); votes.first) {
        period_data.previous_block_cert_votes = std::move(votes.second);
      } else {
        // checkRewardVotes could fail because we just cert voted this block and moved to next period,
        // in that case we are probably fully synced
        if (pbft_block_period <= vote_mgr_->getRewardVotesPbftBlockPeriod()) {
          pbft_syncing_state_->setPbftSyncing(false);
          return;
        }

        LOG(log_er_) << "Invalid reward votes in block " << period_data.pbft_blk->getBlockHash() << " from peer "
                     << packet_data.from_node_id_.abridged() << " received, stop syncing.";
        handleMaliciousSyncPeer(packet_data.from_node_id_);
        return;
      }
    }

    LOG(log_tr_) << "Synced PBFT block hash " << pbft_blk_hash << " with "
                 << period_data.previous_block_cert_votes.size() << " cert votes";
    LOG(log_tr_) << "Synced PBFT block " << period_data;
    pbft_mgr_->periodDataQueuePush(std::move(period_data), packet_data.from_node_id_,
                                   std::move(current_block_cert_votes));
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();

  // Reset last sync packet received time
  pbft_syncing_state_->setLastSyncPacketTime();

  if (pbft_chain_synced) {
    pbftSyncComplete();
    return;
  }

  if (last_block) {
    // If current sync period is actually bigger than the block we just received we are probably synced
    if (pbft_sync_period > pbft_block_period) {
      pbft_syncing_state_->setPbftSyncing(false);
      return;
    }
    if (pbft_syncing_state_->isPbftSyncing()) {
      if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * kConf.network.sync_level_size)) {
        LOG(log_tr_) << "Syncing pbft blocks too fast than processing. Has synced period " << pbft_sync_period
                     << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
        periodic_events_tp_.post(1000, [this] { delayedPbftSync(1); });
      } else {
        if (!syncPeerPbft(pbft_sync_period + 1, true)) {
          pbft_syncing_state_->setPbftSyncing(false);
          return;
        }
      }
    }
  }
}

PeriodData PbftSyncPacketHandler::decodePeriodData(const dev::RLP &period_data_rlp) const {
  return PeriodData(period_data_rlp);
}

std::vector<std::shared_ptr<Vote>> PbftSyncPacketHandler::decodeVotesBundle(const dev::RLP &votes_bundle_rlp) const {
  return decodeVotesBundleRlp(votes_bundle_rlp);
}

void PbftSyncPacketHandler::pbftSyncComplete() {
  if (pbft_mgr_->periodDataQueueSize()) {
    LOG(log_tr_) << "Syncing pbft blocks faster than processing. Remaining sync size "
                 << pbft_mgr_->periodDataQueueSize();
    periodic_events_tp_.post(1000, [this] { pbftSyncComplete(); });
  } else {
    LOG(log_dg_) << "Syncing PBFT is completed";
    // We are pbft synced with the node we are connected to but
    // calling startSyncingPbft will check if some nodes have
    // greater pbft chain size and we should continue syncing with
    // them, Or sync pending DAG blocks
    pbft_syncing_state_->setPbftSyncing(false);
    startSyncingPbft();
    if (!pbft_syncing_state_->isPbftSyncing()) {
      requestPendingDagBlocks();
    }
  }
}

void PbftSyncPacketHandler::delayedPbftSync(int counter) {
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (counter > 60) {
    LOG(log_er_) << "Pbft blocks stuck in queue, no new block processed in 60 seconds " << pbft_sync_period << " "
                 << pbft_chain_->getPbftChainSize();
    pbft_syncing_state_->setPbftSyncing(false);
    LOG(log_tr_) << "Syncing PBFT is stopping";
    return;
  }

  if (pbft_syncing_state_->isPbftSyncing()) {
    if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * kConf.network.sync_level_size)) {
      LOG(log_tr_) << "Syncing pbft blocks faster than processing " << pbft_sync_period << " "
                   << pbft_chain_->getPbftChainSize();
      periodic_events_tp_.post(1000, [this, counter] { delayedPbftSync(counter + 1); });
    } else {
      if (!syncPeerPbft(pbft_sync_period + 1)) {
        pbft_syncing_state_->setPbftSyncing(false);
      }
    }
  }
}

void PbftSyncPacketHandler::handleMaliciousSyncPeer(const dev::p2p::NodeID &id) {
  peers_state_->set_peer_malicious(id);

  if (auto host = peers_state_->host_.lock(); host) {
    LOG(log_nf_) << "Disconnect peer " << id;
    host->disconnect(id, dev::p2p::UserReason);
  } else {
    LOG(log_er_) << "Unable to handleMaliciousSyncPeer, host == nullptr";
  }
}

}  // namespace taraxa::network::tarcap
