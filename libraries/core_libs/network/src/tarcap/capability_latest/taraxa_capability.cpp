#include "network/tarcap/capability_latest/taraxa_capability.hpp"

#include "network/tarcap/capability_latest/packets_handlers/dag_block_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/dag_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/get_next_votes_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/get_pbft_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/pbft_sync_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/status_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/transaction_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/vote_packet_handler.hpp"
#include "network/tarcap/capability_latest/packets_handlers/votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

void TaraxaCapability::registerPacketHandlers(
    const h256 &genesis_hash, const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats,
    const std::shared_ptr<DbStorage> &db, const std::shared_ptr<PbftManager> &pbft_mgr,
    const std::shared_ptr<PbftChain> &pbft_chain, const std::shared_ptr<VoteManager> &vote_mgr,
    const std::shared_ptr<DagManager> &dag_mgr, const std::shared_ptr<TransactionManager> &trx_mgr,
    const addr_t &node_addr) {
  node_stats_ = std::make_shared<tarcap::NodeStats>(peers_state_, pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr,
                                                    vote_mgr, trx_mgr, packets_stats, thread_pool_, node_addr);

  // Register all packet handlers

  // Consensus packets with high processing priority
  packets_handlers_->registerHandler<tarcap::VotePacketHandler>(kConf, peers_state_, packets_stats, pbft_mgr,
                                                                pbft_chain, vote_mgr, node_addr);
  packets_handlers_->registerHandler<tarcap::GetNextVotesBundlePacketHandler>(
      kConf, peers_state_, packets_stats, pbft_mgr, pbft_chain, vote_mgr, node_addr);
  packets_handlers_->registerHandler<tarcap::VotesBundlePacketHandler>(kConf, peers_state_, packets_stats, pbft_mgr,
                                                                       pbft_chain, vote_mgr, node_addr);

  // Standard packets with mid processing priority
  packets_handlers_->registerHandler<tarcap::DagBlockPacketHandler>(kConf, peers_state_, packets_stats,
                                                                    pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr,
                                                                    trx_mgr, db, test_state_, node_addr);

  packets_handlers_->registerHandler<tarcap::TransactionPacketHandler>(kConf, peers_state_, packets_stats, trx_mgr,
                                                                       test_state_, node_addr);

  // Non critical packets with low processing priority
  packets_handlers_->registerHandler<tarcap::StatusPacketHandler>(kConf, peers_state_, packets_stats,
                                                                  pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr,
                                                                  db, genesis_hash, node_addr);
  packets_handlers_->registerHandler<tarcap::GetDagSyncPacketHandler>(kConf, peers_state_, packets_stats, trx_mgr,
                                                                      dag_mgr, db, node_addr);

  packets_handlers_->registerHandler<tarcap::DagSyncPacketHandler>(
      kConf, peers_state_, packets_stats, pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr, trx_mgr, db, node_addr);

  packets_handlers_->registerHandler<tarcap::GetPbftSyncPacketHandler>(
      kConf, peers_state_, packets_stats, pbft_syncing_state_, pbft_chain, vote_mgr, db, node_addr);

  packets_handlers_->registerHandler<tarcap::PbftSyncPacketHandler>(kConf, peers_state_, packets_stats,
                                                                    pbft_syncing_state_, pbft_chain, pbft_mgr, dag_mgr,
                                                                    vote_mgr, periodic_events_tp_, db, node_addr);
}
}  // namespace taraxa::network::tarcap
