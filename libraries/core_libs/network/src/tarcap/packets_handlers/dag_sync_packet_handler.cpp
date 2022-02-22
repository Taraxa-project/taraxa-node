#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<SyncingState> syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr,
                                           std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                                           const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagSyncPacketHandler::validatePacketRlpFormat(const PacketData& packet_data) {
  checkPacketRlpList(packet_data);

  if (size_t required_min_size = 3; packet_data.rlp_.itemCount() < required_min_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_min_size);
  }

  // TODO: rlp format of this packet should be fixed:
  //       has format: [request_period, response_period, transactions_count, tx1, ..., txN, dag1, ...., dagN]
  //       should have format: [request_period, response_period, [tx1, ..., txN], [dag1, ...., dagN] ]

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void DagSyncPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  std::string received_dag_blocks_str;

  auto it = packet_data.rlp_.begin();
  const auto request_period = (*it++).toInt<uint64_t>();
  const auto response_period = (*it++).toInt<uint64_t>();

  // If the periods did not match restart syncing
  if (response_period > request_period) {
    LOG(log_dg_) << "Received DagSyncPacket with mismatching periods: " << response_period << " " << request_period
                 << " from " << packet_data.from_node_id_.abridged();
    if (peer->pbft_chain_size_ < response_period) {
      peer->pbft_chain_size_ = response_period;
    }
    peer->peer_dag_syncing_ = false;
    // We might be behind, restart pbft sync if needed
    restartSyncingPbft();
    return;
  } else if (response_period < request_period) {
    // This should not be possible for honest node
    std::ostringstream err_msg;
    err_msg << "Received DagSyncPacket with mismatching periods: response_period(" << response_period
            << ") != request_period(" << request_period << ")";

    throw MaliciousPeerException(err_msg.str());
  }

  uint64_t transactions_count = (*it++).toInt<uint64_t>();
  SharedTransactions new_transactions;
  std::string transactions_to_log;
  for (uint64_t i = 0; i < transactions_count; i++) {
    std::shared_ptr<Transaction> trx;

    try {
      trx = std::make_shared<Transaction>(*it++);
    } catch (const Transaction::InvalidSignature& e) {
      throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
    }

    peer->markTransactionAsKnown(trx->getHash());
    transactions_to_log += trx->getHash().abridged();
    if (trx_mgr_->markTransactionSeen(trx->getHash())) {
      continue;
    }

    if (const auto [is_valid, reason] = trx_mgr_->verifyTransaction(trx); !is_valid) {
      std::ostringstream err_msg;
      err_msg << "DagBlock transaction " << trx->getHash() << " validation failed: " << reason;

      throw MaliciousPeerException(err_msg.str());
    }

    new_transactions.push_back(std::move(trx));
  }

  trx_mgr_->insertValidatedTransactions(std::move(new_transactions));

  for (; it != packet_data.rlp_.end();) {
    DagBlock block(*it++);
    peer->markDagBlockAsKnown(block.getHash());

    received_dag_blocks_str += block.getHash().abridged() + " ";

    auto status = checkDagBlockValidation(block);
    if (!status.first) {
      // This should only happen with a malicious node or a fork
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block.getHash() << " validation failed: " << status.second;

      throw MaliciousPeerException(err_msg.str());
    }

    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();

    dag_blk_mgr_->insertAndVerifyBlock(std::move(block));
  }

  peer->peer_dag_synced_ = true;
  peer->peer_dag_syncing_ = false;

  LOG(log_dg_) << "Received DagSyncPacket with blocks: " << received_dag_blocks_str
               << " Transactions: " << transactions_to_log << " from " << packet_data.from_node_id_;
}

}  // namespace taraxa::network::tarcap
