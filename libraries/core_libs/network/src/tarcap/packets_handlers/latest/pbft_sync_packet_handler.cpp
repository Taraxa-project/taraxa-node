#include "network/tarcap/packets_handlers/latest/pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa::network::tarcap {

PbftSyncPacketHandler::PbftSyncPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                                             const addr_t &node_addr, const std::string &logs_prefix)
    : ISyncPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                         std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                         logs_prefix + "PBFT_SYNC_PH"),
      vote_mgr_(std::move(vote_mgr)),
      periodic_events_tp_(1, true) {}

void PbftSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                    const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<PbftSyncPacket>(packet_data.rlp_);

  // Note: no need to consider possible race conditions due to concurrent processing as it is
  // disabled on priority_queue blocking dependencies level
  const auto syncing_peer = pbft_syncing_state_->syncingPeer();
  if (!syncing_peer) {
    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << peer->getId().abridged()
                 << " but there is no current syncing peer set";
    return;
  }

  if (syncing_peer->getId() != peer->getId()) {
    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << peer->getId().abridged()
                 << " current syncing peer " << syncing_peer->getId().abridged();
    return;
  }

  // Process received pbft blocks
  // pbft_chain_synced is the flag to indicate own PBFT chain has synced with the peer's PBFT chain
  // TODO: check if pbft_chain_synced usage didn't change after fragaria hf
  const bool pbft_chain_synced = packet.last_block && packet.current_block_cert_votes_bundle.has_value();
  const auto pbft_blk_hash = packet.period_data.pbft_blk->getBlockHash();

  std::string received_dag_blocks_str;  // This is just log related stuff
  for (auto const &block : packet.period_data.dag_blocks) {
    received_dag_blocks_str += block->getHash().toString() + " ";
    if (peer->dag_level_ < block->getLevel()) {
      peer->dag_level_ = block->getLevel();
    }
  }

  const auto pbft_block_period = packet.period_data.pbft_blk->getPeriod();
  LOG(log_dg_) << "PbftSyncPacket received. Period: " << pbft_block_period
               << ", dag Blocks: " << received_dag_blocks_str << " from " << peer->getId();

  peer->markPbftBlockAsKnown(pbft_blk_hash);
  // Update peer's pbft period if outdated
  if (peer->pbft_chain_size_ < pbft_block_period) {
    peer->pbft_chain_size_ = pbft_block_period;
  }

  LOG(log_tr_) << "Processing pbft block: " << pbft_blk_hash;

  if (pbft_chain_->findPbftBlockInChain(pbft_blk_hash)) {
    LOG(log_wr_) << "PBFT block " << pbft_blk_hash << ", period: " << packet.period_data.pbft_blk->getPeriod()
                 << " from " << peer->getId() << " already present in chain";
  } else {
    if (pbft_block_period != pbft_mgr_->pbftSyncingPeriod() + 1) {
      // This can happen if we just got synced and block was cert voted
      if (pbft_chain_synced && pbft_block_period == pbft_mgr_->pbftSyncingPeriod()) {
        pbftSyncComplete();
        return;
      }

      LOG(log_er_) << "Block " << pbft_blk_hash << " period unexpected: " << pbft_block_period
                   << ". Expected period: " << pbft_mgr_->pbftSyncingPeriod() + 1;
      return;
    }

    // Check cert vote matches if final synced block
    if (packet.current_block_cert_votes_bundle.has_value()) {
      for (auto const &vote : packet.current_block_cert_votes_bundle->votes) {
        if (vote->getBlockHash() != pbft_blk_hash) {
          LOG(log_er_) << "Invalid cert votes block hash " << vote->getBlockHash() << " instead of " << pbft_blk_hash
                       << " from peer " << peer->getId().abridged() << " received, stop syncing.";
          peers_state_->handleMaliciousSyncPeer(peer->getId());
          return;
        }
      }
    }

    auto required_block_hash = kConf.genesis.state.hardforks.isOnFragariaHardfork(pbft_block_period)
                                   ? pbft_mgr_->secondLastPbftBlockHashFromQueueOrChain(pbft_block_period)
                                   : pbft_mgr_->lastPbftBlockHashFromQueueOrChain();
    for (auto const &vote : packet.period_data.reward_votes_) {
      if (vote->getBlockHash() != required_block_hash) {
        LOG(log_er_) << "Pbft block " << packet.period_data.pbft_blk->getBlockHash() << ", period "
                     << packet.period_data.pbft_blk->getPeriod() << ". Invalid reward vote block hash "
                     << vote->getBlockHash() << " instead of " << required_block_hash << " from peer "
                     << peer->getId().abridged() << " received, stop syncing.";

        peers_state_->handleMaliciousSyncPeer(peer->getId());
        return;
      }
    }

    if (!pbft_mgr_->validatePillarDataInPeriodData(packet.period_data)) {
      peers_state_->handleMaliciousSyncPeer(peer->getId());
      return;
    }

    auto order_hash = PbftManager::calculateOrderHash(packet.period_data.dag_blocks);
    if (order_hash != packet.period_data.pbft_blk->getOrderHash()) {
      {  // This is just log related stuff
        std::vector<trx_hash_t> trx_order;
        trx_order.reserve(packet.period_data.transactions.size());
        std::vector<blk_hash_t> blk_order;
        blk_order.reserve(packet.period_data.dag_blocks.size());
        for (auto t : packet.period_data.transactions) {
          trx_order.push_back(t->getHash());
        }
        for (auto b : packet.period_data.dag_blocks) {
          blk_order.push_back(b->getHash());
        }
        LOG(log_er_) << "Order hash incorrect in period data " << pbft_blk_hash << " expected: " << order_hash
                     << " received " << packet.period_data.pbft_blk->getOrderHash() << "; Dag order: " << blk_order
                     << "; Trx order: " << trx_order << "; from " << peer->getId().abridged() << ", stop syncing.";
      }
      peers_state_->handleMaliciousSyncPeer(peer->getId());
      return;
    }

    // This is special case when queue is empty and we can not say for sure that all votes that are part of this block
    // have been verified before
    if (pbft_mgr_->periodDataQueueEmpty()) {
      for (const auto &v : packet.period_data.reward_votes_) {
        if (auto vote_is_valid = vote_mgr_->validateVote(v); vote_is_valid.first == false) {
          LOG(log_er_) << "Invalid reward votes in block " << packet.period_data.pbft_blk->getBlockHash()
                       << " from peer " << peer->getId().abridged()
                       << " received, stop syncing. Validation failed. Err: " << vote_is_valid.second;
          peers_state_->handleMaliciousSyncPeer(peer->getId());
          return;
        }

        vote_mgr_->addVerifiedVote(v);
      }

      // And now we need to replace it with verified votes
      if (auto votes = vote_mgr_->checkRewardVotes(packet.period_data.pbft_blk, true); votes.first) {
        packet.period_data.reward_votes_ = std::move(votes.second);
      } else {
        // checkRewardVotes could fail because we just cert voted this block and moved to next period,
        // in that case we are probably fully synced
        // TODO: check related to fragaria hf
        if (pbft_block_period <= vote_mgr_->getRewardVotesPeriod(packet.period_data.pbft_blk->getPeriod())) {
          pbft_syncing_state_->setPbftSyncing(false);
          return;
        }

        LOG(log_er_) << "Invalid reward votes in block " << packet.period_data.pbft_blk->getBlockHash() << " from peer "
                     << peer->getId().abridged() << " received, stop syncing.";
        peers_state_->handleMaliciousSyncPeer(peer->getId());
        return;
      }
    }

    LOG(log_tr_) << "Synced PBFT block hash " << pbft_blk_hash << " with " << packet.period_data.reward_votes_.size()
                 << " reward votes";
    std::vector<std::shared_ptr<PbftVote>> current_block_cert_votes;
    if (packet.current_block_cert_votes_bundle.has_value()) {
      current_block_cert_votes = std::move(packet.current_block_cert_votes_bundle->votes);
    }

    pbft_mgr_->periodDataQueuePush({peer->getId(), std::move(packet.period_data), std::move(current_block_cert_votes)});
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();

  // Reset last sync packet received time
  pbft_syncing_state_->setLastSyncPacketTime();

  if (pbft_chain_synced) {
    pbftSyncComplete();
    return;
  }

  if (packet.last_block) {
    // If current sync period is actually bigger than the block we just received we are probably synced
    if (pbft_sync_period > pbft_block_period) {
      pbft_syncing_state_->setPbftSyncing(false);
      return;
    }
    if (pbft_syncing_state_->isPbftSyncing()) {
      if (pbft_sync_period > pbft_chain_->getPbftChainSize() + (10 * kConf.network.sync_level_size)) {
        LOG(log_tr_) << "Syncing pbft blocks too fast than processing. Has synced period " << pbft_sync_period
                     << ", PBFT chain size " << pbft_chain_->getPbftChainSize();
        periodic_events_tp_.post(kDelayedPbftSyncDelayMs, [this] { delayedPbftSync(1); });
      } else {
        if (!syncPeerPbft(pbft_sync_period + 1)) {
          pbft_syncing_state_->setPbftSyncing(false);
          return;
        }
      }
    }
  }
}

void PbftSyncPacketHandler::pbftSyncComplete() {
  if (pbft_mgr_->periodDataQueueSize()) {
    LOG(log_tr_) << "Syncing pbft blocks faster than processing. Remaining sync size "
                 << pbft_mgr_->periodDataQueueSize();
    periodic_events_tp_.post(kDelayedPbftSyncDelayMs, [this] { pbftSyncComplete(); });
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

void PbftSyncPacketHandler::delayedPbftSync(uint32_t counter) {
  const uint32_t max_delayed_pbft_sync_count = 60000 / kDelayedPbftSyncDelayMs;
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (counter > max_delayed_pbft_sync_count) {
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
      periodic_events_tp_.post(kDelayedPbftSyncDelayMs, [this, counter] { delayedPbftSync(counter + 1); });
    } else {
      if (!syncPeerPbft(pbft_sync_period + 1)) {
        pbft_syncing_state_->setPbftSyncing(false);
      }
    }
  }
}

}  // namespace taraxa::network::tarcap
