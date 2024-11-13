#include "network/tarcap/packets_handlers/v4/dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/v4/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap::v4 {

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

void DagSyncPacketHandler::validatePacketRlpFormat(const threadpool::PacketData& packet_data) const {
  if (constexpr size_t required_size = 4; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void DagSyncPacketHandler::process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  auto it = packet_data.rlp_.begin();
  const auto request_period = (*it++).toInt<PbftPeriod>();
  const auto response_period = (*it++).toInt<PbftPeriod>();

  // If the periods did not match restart syncing
  if (response_period > request_period) {
    LOG(log_dg_) << "Received DagSyncPacket with mismatching periods: " << response_period << " " << request_period
                 << " from " << packet_data.from_node_id_.abridged();
    if (peer->pbft_chain_size_ < response_period) {
      peer->pbft_chain_size_ = response_period;
    }
    peer->peer_dag_syncing_ = false;
    // We might be behind, restart pbft sync if needed
    startSyncingPbft();
    return;
  } else if (response_period < request_period) {
    // This should not be possible for honest node
    std::ostringstream err_msg;
    err_msg << "Received DagSyncPacket with mismatching periods: response_period(" << response_period
            << ") != request_period(" << request_period << ")";

    throw MaliciousPeerException(err_msg.str());
  }

  std::vector<trx_hash_t> transactions_to_log;
  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> transactions;
  const auto trx_count = (*it).itemCount();
  transactions.reserve(trx_count);
  transactions_to_log.reserve(trx_count);

  for (const auto tx_rlp : (*it++)) {
    try {
      auto trx = std::make_shared<Transaction>(tx_rlp);
      peer->markTransactionAsKnown(trx->getHash());
      transactions.emplace(trx->getHash(), std::move(trx));
    } catch (const Transaction::InvalidTransaction& e) {
      throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
    }
  }

  std::vector<std::shared_ptr<DagBlock>> dag_blocks;
  std::vector<blk_hash_t> dag_blocks_to_log;
  dag_blocks.reserve((*it).itemCount());
  dag_blocks_to_log.reserve((*it).itemCount());

  for (const auto block_rlp : *it) {
    auto block = std::make_shared<DagBlock>(block_rlp);
    peer->markDagBlockAsKnown(block->getHash());
    if (dag_mgr_->isDagBlockKnown(block->getHash())) {
      LOG(log_tr_) << "Received known DagBlock " << block->getHash() << "from: " << peer->getId();
      continue;
    }
    dag_blocks.emplace_back(std::move(block));
  }

  for (auto& trx : transactions) {
    transactions_to_log.push_back(trx.first);
    if (trx_mgr_->isTransactionKnown(trx.first)) {
      continue;
    }

    auto [verified, reason] = trx_mgr_->verifyTransaction(trx.second);
    if (!verified) {
      std::ostringstream err_msg;
      err_msg << "DagBlock transaction " << trx.first << " validation failed: " << reason;
      throw MaliciousPeerException(err_msg.str());
    }
  }

  for (auto& block : dag_blocks) {
    dag_blocks_to_log.push_back(block->getHash());

    auto verified = dag_mgr_->verifyBlock(block, transactions);
    if (verified.first != DagManager::VerifyBlockReturnType::Verified) {
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block->getHash() << " failed verification with error code "
              << static_cast<uint32_t>(verified.first);
      throw MaliciousPeerException(err_msg.str());
    }

    if (block->getLevel() > peer->dag_level_) peer->dag_level_ = block->getLevel();

    auto status = dag_mgr_->addDagBlock(std::move(block), std::move(verified.second));
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
               << " Transactions: " << transactions_to_log << " from " << packet_data.from_node_id_;
}

}  // namespace taraxa::network::tarcap::v4
