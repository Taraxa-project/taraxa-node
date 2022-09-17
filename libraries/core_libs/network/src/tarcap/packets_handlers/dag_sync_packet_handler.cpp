#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                                           const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagSyncPacketHandler::validatePacketRlpFormat(const PacketData& packet_data) const {
  if (constexpr size_t required_size = 4; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
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

  std::string transactions_to_log;

  for (const auto tx_rlp : (*it++)) {
    std::shared_ptr<Transaction> trx;

    try {
      trx = std::make_shared<Transaction>(tx_rlp);
    } catch (const Transaction::InvalidSignature& e) {
      throw MaliciousPeerException("Unable to parse transaction: " + std::string(e.what()));
    }

    peer->markTransactionAsKnown(trx->getHash());
    transactions_to_log += trx->getHash().abridged();
    if (trx_mgr_->isTransactionKnown(trx->getHash())) {
      continue;
    }

    const auto [status, reason] = trx_mgr_->verifyTransaction(trx);
    switch (status) {
      case TransactionStatus::Invalid: {
        std::ostringstream err_msg;
        err_msg << "DagBlock transaction " << trx->getHash() << " validation failed: " << reason;
        throw MaliciousPeerException(err_msg.str());
      }
      case TransactionStatus::InsufficentBalance:
      case TransactionStatus::LowNonce: {
        if (peer->reportSuspiciousPacket()) {
          std::ostringstream err_msg;
          err_msg << "Suspicious packets over the limit on DagBlock transaction " << trx->getHash()
                  << " validation: " << reason;
          throw MaliciousPeerException(err_msg.str());
        }
        break;
      }
      case TransactionStatus::Verified:
        break;
      default:
        assert(false);
    }
    if (!trx_mgr_->isTransactionPoolFull()) [[likely]] {
      trx_mgr_->insertValidatedTransaction(std::move(trx), std::move(status));
    } else {
      trx_mgr_->insertValidatedTransaction(std::move(trx), std::move(TransactionStatus::Forced));
    }
  }

  for (const auto block_rlp : *it) {
    DagBlock block(block_rlp);
    peer->markDagBlockAsKnown(block.getHash());

    received_dag_blocks_str += block.getHash().abridged() + " ";

    const auto verified = dag_mgr_->verifyBlock(block);
    if (verified != DagManager::VerifyBlockReturnType::Verified) {
      std::ostringstream err_msg;
      err_msg << "DagBlock" << block.getHash() << " failed verification with error code "
              << static_cast<uint32_t>(verified);
      throw MaliciousPeerException(err_msg.str());
    }

    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();

    auto transactions = trx_mgr_->getPoolTransactions(block.getTrxs()).first;
    auto status = dag_mgr_->addDagBlock(std::move(block), std::move(transactions));
    if (!status.first) {
      std::ostringstream err_msg;
      if (status.second.size() > 0)
        err_msg << "DagBlock" << block.getHash() << " has missing pivot or/and tips " << status.second;
      else
        err_msg << "DagBlock" << block.getHash() << " could not be added to DAG";
      throw MaliciousPeerException(err_msg.str());
    }
  }

  peer->peer_dag_synced_ = true;
  peer->peer_dag_syncing_ = false;

  LOG(log_dg_) << "Received DagSyncPacket with blocks: " << received_dag_blocks_str
               << " Transactions: " << transactions_to_log << " from " << packet_data.from_node_id_;
}

}  // namespace taraxa::network::tarcap
