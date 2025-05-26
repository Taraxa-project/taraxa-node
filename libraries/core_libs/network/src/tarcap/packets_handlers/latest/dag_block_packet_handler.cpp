#include "network/tarcap/packets_handlers/latest/dag_block_packet_handler.hpp"

#include "dag/dag_manager.hpp"
#include "network/tarcap/packets_handlers/latest/transaction_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagBlockPacketHandler::DagBlockPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                             std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                             std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                             std::shared_ptr<PbftChain> pbft_chain,
                                             std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                                             std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db,
                                             const std::string &logs_prefix)
    : IDagBlockPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                             std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db),
                             logs_prefix + "DAG_BLOCK_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagBlockPacketHandler::process(const threadpool::PacketData &packet_data,
                                    const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<DagBlockPacket>(packet_data.rlp_);

  blk_hash_t const hash = packet.dag_block->getHash();

  for (const auto &tx : packet.transactions) {
    peer->markTransactionAsKnown(tx->getHash());
  }
  peer->markDagBlockAsKnown(hash);

  if (packet.dag_block->getLevel() > peer->dag_level_) {
    peer->dag_level_ = packet.dag_block->getLevel();
  }

  // Do not process this block in case we already have it
  if (dag_mgr_->isDagBlockKnown(packet.dag_block->getHash())) {
    logger_->trace("Received known DagBlockPacket {} from: {}", hash, peer->getId());
    return;
  }

  std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> txs_map;
  txs_map.reserve(packet.transactions.size());
  for (const auto &tx : packet.transactions) {
    txs_map.emplace(tx->getHash(), tx);
  }

  onNewBlockReceived(std::move(packet.dag_block), peer, txs_map);
}

void DagBlockPacketHandler::sendBlockWithTransactions(const std::shared_ptr<TaraxaPeer> &peer,
                                                      const std::shared_ptr<DagBlock> &block,
                                                      SharedTransactions &&trxs) {
  // This lock prevents race condition between syncing and gossiping dag blocks
  std::unique_lock lock(peer->mutex_for_sending_dag_blocks_);

  DagBlockPacket dag_block_packet{.transactions = std::move(trxs), .dag_block = block};
  if (!sealAndSend(peer->getId(), SubprotocolPacketType::kDagBlockPacket, encodePacketRlp(dag_block_packet))) {
    logger_->warn("Sending DagBlock {} failed to {}", block->getHash(), peer->getId());
    return;
  }

  // Mark data as known if sending was successful
  peer->markDagBlockAsKnown(block->getHash());
}

void DagBlockPacketHandler::onNewBlockReceived(
    std::shared_ptr<DagBlock> &&block, const std::shared_ptr<TaraxaPeer> &peer,
    const std::unordered_map<trx_hash_t, std::shared_ptr<Transaction>> &trxs) {
  const auto block_hash = block->getHash();
  auto verified = dag_mgr_->verifyBlock(block, trxs);
  switch (verified.first) {
    case DagManager::VerifyBlockReturnType::IncorrectTransactionsEstimation:
    case DagManager::VerifyBlockReturnType::BlockTooBig:
    case DagManager::VerifyBlockReturnType::FailedVdfVerification:
    case DagManager::VerifyBlockReturnType::NotEligible:
    case DagManager::VerifyBlockReturnType::FailedTipsVerification: {
      std::ostringstream err_msg;
      err_msg << "DagBlock " << block_hash << " failed verification with error code "
              << static_cast<uint32_t>(verified.first);
      throw MaliciousPeerException(err_msg.str());
    }
    case DagManager::VerifyBlockReturnType::MissingTransaction:
      if (peer->dagSyncingAllowed()) {
        if (trx_mgr_->transactionsDropped()) [[unlikely]] {
          logger_->info(
              "NewBlock {} from peer {} is missing transaction, our pool recently dropped transactions, requesting dag "
              "sync",
              block_hash.toString(), peer->getId());
        } else {
          logger_->warn("NewBlock {} from peer {} is missing transaction, requesting dag sync", block_hash.toString(),
                        peer->getId());
        }
        peer->peer_dag_synced_ = false;
        requestPendingDagBlocks(peer);
      } else {
        if (trx_mgr_->transactionsDropped()) [[unlikely]] {
          // Disconnecting since anything after will also contain missing pivot/tips ...
          logger_->info(
              "NewBlock {} from peer {} is missing transaction, but our pool recently dropped transactions, "
              "disconnecting",
              block_hash.toString(), peer->getId());
          disconnect(peer->getId(), dev::p2p::UserReason);
        } else {
          std::ostringstream err_msg;
          err_msg << "DagBlock " << block_hash << " is missing a transaction while in a dag synced state";
          throw MaliciousPeerException(err_msg.str());
        }
      }
      break;
    case DagManager::VerifyBlockReturnType::MissingTip:
      if (peer->peer_dag_synced_) {
        if (peer->dagSyncingAllowed()) {
          logger_->warn("NewBlock {} from peer {} is missing tip, requesting dag sync", block_hash.toString(),
                        peer->getId());
          peer->peer_dag_synced_ = false;
          requestPendingDagBlocks(peer);
        } else {
          std::ostringstream err_msg;
          err_msg << "DagBlock has missing tip";
          throw MaliciousPeerException(err_msg.str());
        }
      } else {
        // peer_dag_synced_ flag ensures that this can only be performed once for a peer
        requestPendingDagBlocks(peer);
      }
      break;
    case DagManager::VerifyBlockReturnType::AheadBlock:
    case DagManager::VerifyBlockReturnType::FutureBlock:
      if (peer->peer_dag_synced_) {
        logger_->error("DagBlock {} is an ahead/future block. Peer {} will be disconnected", block_hash, peer->getId());
        disconnect(peer->getId(), dev::p2p::UserReason);
      }
      break;
    case DagManager::VerifyBlockReturnType::Verified: {
      auto status = dag_mgr_->addDagBlock(block, std::move(verified.second));
      if (!status.first) {
        logger_->debug("Received DagBlockPacket {} from: {}", block_hash, peer->getId());
        // Ignore new block packets when pbft syncing
        if (pbft_syncing_state_->isPbftSyncing()) {
          logger_->debug("Ignore new dag block {}, pbft syncing is on", block_hash);
        } else if (peer->peer_dag_syncing_) {
          logger_->debug("Ignore new dag block {}, dag syncing is on", block_hash);
        } else {
          if (peer->peer_dag_synced_) {
            std::ostringstream err_msg;
            if (status.second.size() > 0)
              err_msg << "DagBlock " << block->getHash() << " has missing pivot or/and tips " << status.second;
            else
              err_msg << "DagBlock" << block->getHash() << " could not be added to DAG";
            throw MaliciousPeerException(err_msg.str());
          } else {
            // peer_dag_synced_ flag ensures that this can only be performed once for a peer
            requestPendingDagBlocks(peer);
          }
        }
      }
    } break;
    case DagManager::VerifyBlockReturnType::ExpiredBlock:
      break;
  }
}

}  // namespace taraxa::network::tarcap
