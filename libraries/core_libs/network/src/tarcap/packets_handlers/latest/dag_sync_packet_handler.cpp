#include "network/tarcap/packets_handlers/latest/dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                           std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                                           const addr_t& node_addr, const std::string& logs_prefix)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              logs_prefix + "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagSyncPacketHandler::process(DagSyncPacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) {
  // If the periods did not match restart syncing
  if (packet.response_period > packet.request_period) {
    LOG(log_dg_) << "Received DagSyncPacket with mismatching periods: " << packet.response_period << " "
                 << packet.request_period << " from " << peer->getId();
    if (peer->pbft_chain_size_ < packet.response_period) {
      peer->pbft_chain_size_ = packet.response_period;
    }
    peer->peer_dag_syncing_ = false;
    // We might be behind, restart pbft sync if needed
    startSyncingPbft();
    return;
  } else if (packet.response_period < packet.request_period) {
    // This should not be possible for honest node
    std::ostringstream err_msg;
    err_msg << "Received DagSyncPacket with mismatching periods: response_period(" << packet.response_period
            << ") != request_period(" << packet.request_period << ")";

    throw MaliciousPeerException(err_msg.str());
  }

  std::vector<trx_hash_t> transactions_to_log;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> transactions_map;
  transactions_to_log.reserve(packet.transactions.size());
  transactions_map.reserve(packet.transactions.size());
  for (auto& trx : packet.transactions) {
    const auto tx_hash = trx->getHash();
    peer->markTransactionAsKnown(tx_hash);
    transactions_to_log.push_back(tx_hash);
    transactions_map.emplace(tx_hash, trx);

    if (trx_mgr_->isTransactionKnown(tx_hash)) {
      continue;
    }

    auto [verified, reason] = trx_mgr_->verifyTransaction(trx);
    if (!verified) {
      std::ostringstream err_msg;
      err_msg << "DagBlock transaction " << tx_hash << " validation failed: " << reason;
      throw MaliciousPeerException(err_msg.str());
    }
  }

  std::vector<blk_hash_t> dag_blocks_to_log;
  dag_blocks_to_log.reserve(packet.dag_blocks.size());
  for (auto& block : packet.dag_blocks) {
    dag_blocks_to_log.push_back(block->getHash());
    peer->markDagBlockAsKnown(block->getHash());

    if (dag_mgr_->isDagBlockKnown(block->getHash())) {
      LOG(log_tr_) << "Received known DagBlock " << block->getHash() << "from: " << peer->getId();
      continue;
    }

    auto verified = dag_mgr_->verifyBlock(*block, transactions_map);
    if (verified.first != DagManager::VerifyBlockReturnType::Verified) {
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block->getHash() << " failed verification with error code "
              << static_cast<uint32_t>(verified.first);
      throw MaliciousPeerException(err_msg.str());
    }

    if (block->getLevel() > peer->dag_level_) peer->dag_level_ = block->getLevel();

    // TODO[2869]: fix dag blocks usage - shared_ptr vs object type on different places...
    auto status = dag_mgr_->addDagBlock(std::move(*block), std::move(verified.second));
    if (!status.first) {
      std::ostringstream err_msg;
      if (status.second.size() > 0)
        err_msg << "DagBlock" << block->getHash() << " has missing pivot or/and tips " << status.second;
      else
        err_msg << "DagBlock" << block->getHash() << " could not be added to DAG";
      throw MaliciousPeerException(err_msg.str());
    }
  }

  peer->peer_dag_synced_ = true;
  peer->peer_dag_synced_time_ =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  peer->peer_dag_syncing_ = false;

  LOG(log_dg_) << "Received DagSyncPacket with blocks: " << dag_blocks_to_log
               << " Transactions: " << transactions_to_log << " from " << peer->getId();
}

}  // namespace taraxa::network::tarcap
