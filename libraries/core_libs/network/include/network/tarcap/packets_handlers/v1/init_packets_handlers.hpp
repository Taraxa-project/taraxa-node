#pragma once

#include "get_next_votes_bundle_packet_handler.hpp"
#include "get_pbft_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/get_dag_sync_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/status_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/packets_handlers/latest/vote_packet_handler.hpp"
#include "network/tarcap/taraxa_capability.hpp"
#include "pbft_sync_packet_handler.hpp"
#include "votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

/**
 * @brief Taraxa capability V1 InitPacketsHandlers function definition
 */
static const TaraxaCapability::InitPacketsHandlers kInitV1Handlers =
    [](const std::string &logs_prefix, const FullNodeConfig &config, const h256 &genesis_hash,
       const std::shared_ptr<PeersState> &peers_state, const std::shared_ptr<PbftSyncingState> &pbft_syncing_state,
       const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats, const std::shared_ptr<DbStorage> &db,
       const std::shared_ptr<PbftManager> &pbft_mgr, const std::shared_ptr<PbftChain> &pbft_chain,
       const std::shared_ptr<VoteManager> &vote_mgr, const std::shared_ptr<DagManager> &dag_mgr,
       const std::shared_ptr<TransactionManager> &trx_mgr, const addr_t &node_addr) {
      auto packets_handlers = std::make_shared<PacketsHandler>();

      // Consensus packets with high processing priority
      packets_handlers->registerHandler<tarcap::VotePacketHandler>(config, peers_state, packets_stats, pbft_mgr,
                                                                   pbft_chain, vote_mgr, node_addr, logs_prefix);
      packets_handlers->registerHandler<tarcap::v1::GetNextVotesBundlePacketHandler>(
          config, peers_state, packets_stats, pbft_mgr, pbft_chain, vote_mgr, node_addr, logs_prefix);
      packets_handlers->registerHandler<tarcap::v1::VotesBundlePacketHandler>(
          config, peers_state, packets_stats, pbft_mgr, pbft_chain, vote_mgr, node_addr, logs_prefix);

      // Standard packets with mid processing priority
      packets_handlers->registerHandler<tarcap::DagBlockPacketHandler>(config, peers_state, packets_stats,
                                                                       pbft_syncing_state, pbft_chain, pbft_mgr,
                                                                       dag_mgr, trx_mgr, db, node_addr, logs_prefix);

      packets_handlers->registerHandler<tarcap::TransactionPacketHandler>(config, peers_state, packets_stats, trx_mgr,
                                                                          node_addr, logs_prefix);

      // Non critical packets with low processing priority
      packets_handlers->registerHandler<tarcap::StatusPacketHandler>(config, peers_state, packets_stats,
                                                                     pbft_syncing_state, pbft_chain, pbft_mgr, dag_mgr,
                                                                     db, genesis_hash, node_addr, logs_prefix);
      packets_handlers->registerHandler<tarcap::GetDagSyncPacketHandler>(config, peers_state, packets_stats, trx_mgr,
                                                                         dag_mgr, db, node_addr, logs_prefix);

      packets_handlers->registerHandler<tarcap::DagSyncPacketHandler>(config, peers_state, packets_stats,
                                                                      pbft_syncing_state, pbft_chain, pbft_mgr, dag_mgr,
                                                                      trx_mgr, db, node_addr, logs_prefix);

      packets_handlers->registerHandler<tarcap::v1::GetPbftSyncPacketHandler>(
          config, peers_state, packets_stats, pbft_syncing_state, pbft_chain, vote_mgr, db, node_addr, logs_prefix);

      packets_handlers->registerHandler<tarcap::v1::PbftSyncPacketHandler>(
          config, peers_state, packets_stats, pbft_syncing_state, pbft_chain, pbft_mgr, dag_mgr, vote_mgr, db,
          node_addr, logs_prefix);

      return packets_handlers;
    };

}  // namespace taraxa::network::tarcap::v1
