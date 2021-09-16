#include "transaction_manager.hpp"

#include <string>
#include <utility>

#include "dag/dag.hpp"
#include "logger/log.hpp"
#include "network/network.hpp"
#include "network/rpc/WSServer.h"
#include "transaction.hpp"

using namespace taraxa::net;
namespace taraxa {
auto trxComp = [](Transaction const &t1, Transaction const &t2) -> bool {
  if (t1.getSender() < t2.getSender())
    return true;
  else if (t1.getSender() == t2.getSender()) {
    return t1.getNonce() < t2.getNonce();
  } else {
    return false;
  }
};

TransactionManager::TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                                       VerifyMode mode)
    : mode_(mode), conf_(conf), trx_qu_(node_addr), seen_txs_(200000 /*capacity*/, 2000 /*delete step*/), db_(db) {
  LOG_OBJECTS_CREATE("TRXMGR");
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  trx_count_.store(trx_count);
}

TransactionManager::~TransactionManager() { stop(); }

std::pair<bool, std::string> TransactionManager::verifyTransaction(Transaction const &trx) const {
  if (trx.getChainID() != conf_.chain.chain_id) {
    return {false, "chain_id mismatch"};
  }
  try {
    trx.getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {false, "invalid signature"};
  }
  return {true, ""};
}

bool TransactionManager::checkQueueOverflow() {
  const auto queues_sizes = trx_qu_.getTransactionQueueSize();
  size_t combined_queues_size =
      queues_sizes.first /* unverified txs queue */ + queues_sizes.second /* verified txs queue */;

  size_t queue_overflow_warn = conf_.test_params.max_transaction_queue_warn;
  size_t queue_overflow_drop = conf_.test_params.max_transaction_queue_drop;

  auto queuesOverflowWarnMsg = [&combined_queues_size, &queues_sizes, &queue_overflow_drop]() -> std::string {
    std::ostringstream oss;
    oss << "Warning: queues too large: "
        << "Combined queues size: " << combined_queues_size << ", Unverified queue: " << queues_sizes.first
        << ", Verified queue: " << queues_sizes.second << ", Limit: " << queue_overflow_drop;

    return oss.str();
  };

  if (queue_overflow_drop && combined_queues_size >= queue_overflow_drop) {
    LOG(log_wr_) << queuesOverflowWarnMsg() << " --> New transactions will not be processed !";
    return true;
  } else if (queue_overflow_warn && combined_queues_size >= queue_overflow_warn) {
    LOG(log_wr_) << queuesOverflowWarnMsg() << " --> Only warning, transactions will bed processed.";
  }

  return false;
}

std::pair<bool, std::string> TransactionManager::insertTransaction(const Transaction &trx, bool verify,
                                                                   bool broadcast) {
  const auto &tx_hash = trx.getHash();

  if (checkQueueOverflow() == true) {
    LOG(log_er_) << "Queue overlfow";
    return std::make_pair(false, "Queue overlfow");
  }

  if (verify && mode_ != VerifyMode::skip_verify_sig) {
    if (const auto verified = verifyTransaction(trx); verified.first == false) {
      LOG(log_er_) << "Trying to insert invalid trx, hash: " << tx_hash << ", err msg: " << verified.second;
      return verified;
    }
  }

  {
    // TODO: rework tx_status synchronization...
    std::unique_lock transaction_status_lock(transaction_status_mutex_);
    TransactionStatus status = db_->getTransactionStatus(tx_hash);
    if (status.state != TransactionStatusEnum::not_seen) {
      switch (status.state) {
        case TransactionStatusEnum::in_queue_verified:
          LOG(log_dg_) << "Trx: " << tx_hash << "skip, seen in verified queue. " << std::endl;
          return std::make_pair(false, "in verified queue");
        case TransactionStatusEnum::in_queue_unverified:
          LOG(log_dg_) << "Trx: " << tx_hash << "skip, seen in unverified queue. " << std::endl;
          return std::make_pair(false, "in unverified queue");
        case TransactionStatusEnum::in_block:
          LOG(log_dg_) << "Trx: " << tx_hash << "skip, seen in block. " << std::endl;
          return std::make_pair(false, "in block");
        case TransactionStatusEnum::finalized:
          LOG(log_dg_) << "Trx: " << tx_hash << "skip, finalized " << std::endl;
          return std::make_pair(false, "finalized");
        case TransactionStatusEnum::invalid:
          LOG(log_dg_) << "Trx: " << tx_hash << "skip, seen but invalid. " << std::endl;
          return std::make_pair(false, "already invalid");
        default:
          return std::make_pair(false, "unknown");
      }
    }

    // Insert unseen tx into the queue + database
    // Synchronization point in case multiple threads are processing the same tx at the same time
    // TODO: we have 2 nested locks here, rework tx_status synchronization and its lock...
    bool tx_verified = verify && mode_ != VerifyMode::skip_verify_sig;
    if (!trx_qu_.insert(trx, verify, tx_verified, db_)) {
      return std::make_pair(false, "skip, already inserted by different thread(race condition)");
    }
  }

  // TODO: new pending tx could be omitted here ? Currently we ommit new pending tx only if it is included in dag block
  // TODO: either create different events, e.g. new pending vs new accepted or remove this whole TODO
  //  transaction_accepted_.emit(trx.getHash());

  if (broadcast == true) {
    if (auto net = network_.lock(); net && conf_.network.network_transaction_interval == 0) {
      net->onNewTransactions({trx});
    }
  }

  return std::make_pair(true, "");
}

uint32_t TransactionManager::insertBroadcastedTransactions(const std::vector<Transaction> &txs) {
  if (stopped_) {
    return 0;
  }

  if (checkQueueOverflow() == true) {
    return 0;
  }

  std::vector<Transaction> unseen_txs;
  unseen_txs.reserve(txs.size());

  std::vector<trx_hash_t> txs_hashes;
  txs_hashes.reserve(txs.size());
  std::transform(txs.begin(), txs.end(), std::back_inserter(txs_hashes),
                 [](const Transaction &tx) -> trx_hash_t { return tx.getHash(); });

  {
    // TODO: rework tx_status synchronization...
    std::unique_lock transaction_status_lock(transaction_status_mutex_);

    // Get transactions statuses from db
    DbStorage::MultiGetQuery db_query(db_, txs_hashes.size());
    db_query.append(DbStorage::Columns::trx_status, txs_hashes);
    auto db_trxs_statuses = db_query.execute();

    for (size_t idx = 0; idx < db_trxs_statuses.size(); idx++) {
      auto &trx_raw_status = db_trxs_statuses[idx];
      const trx_hash_t &trx_hash = txs_hashes[idx];

      TransactionStatus trx_status;
      if (!trx_raw_status.empty()) {
        auto data = asBytes(trx_raw_status);
        trx_status = TransactionStatus(RLP(data));
      }

      LOG(log_dg_) << "Broadcasted transaction " << trx_hash << " received at: " << getCurrentTimeMilliSeconds();

      // Trx status was already saved in db -> it means we already processed this trx
      // Do not process it again
      if (trx_status.state != TransactionStatusEnum::not_seen) {
        switch (trx_status.state) {
          case TransactionStatusEnum::in_queue_verified:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in verified queue.";
            break;
          case TransactionStatusEnum::in_queue_unverified:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in unverified queue.";
            break;
          case TransactionStatusEnum::in_block:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen in block.";
            break;
          case TransactionStatusEnum::finalized:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, finalized.";
            break;
          case TransactionStatusEnum::invalid:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, seen but invalid.";
            break;
          default:
            LOG(log_dg_) << "Trx: " << trx_hash << " skipped, unknown trx status saved in db.";
        }

        continue;
      }

      unseen_txs.emplace_back(std::move(txs[idx]));

      // TODO: new pending tx could be omitted here ? Currently we ommit new pending tx only if it is included in dag
      // block
      // TODO: either create different events, e.g. new pending vs new accepted or remove this whole TODO
      //  transaction_accepted_.emit(trx.getHash());
    }

    // Insert unseen txs into the queue + database
    // Synchronization point in case multiple threads are processing the same tx at the same time
    // TODO: we have 2 nested locks here, rework tx_status synchronization and its lock...
    trx_qu_.insertUnverifiedTrxs(unseen_txs, db_);
  }

  LOG(log_nf_) << txs.size() << " received txs processed (" << unseen_txs.size() << " unseen -> inserted into db).";
  return unseen_txs.size();
}

void TransactionManager::verifyQueuedTrxs() {
  while (!stopped_) {
    // It will wait if no transaction in unverified queue
    auto item = trx_qu_.getUnverifiedTransaction();
    if (stopped_) return;

    std::pair<bool, std::string> valid;
    trx_hash_t hash = item.second->getHash();
    // verify and put the transaction to verified queue
    if (mode_ == VerifyMode::skip_verify_sig) {
      valid.first = true;
    } else {
      valid = verifyTransaction(*item.second);
    }
    {
      // TODO: rework tx_status synchronization...
      std::unique_lock transaction_status_lock(transaction_status_mutex_);

      // mark invalid
      if (!valid.first) {
        db_->saveTransactionStatus(hash, TransactionStatusEnum::invalid);
        trx_qu_.removeTransactionFromBuffer(hash);

        LOG(log_wr_) << " Trx: " << hash << "invalid: " << valid.second << std::endl;
        continue;
      }
      auto status = db_->getTransactionStatus(hash);
      if (status.state == TransactionStatusEnum::in_queue_unverified) {
        status.state = TransactionStatusEnum::in_queue_verified;
        db_->saveTransactionStatus(hash, status);
        db_->saveTransaction(*item.second, mode_ != VerifyMode::skip_verify_sig);
        trx_qu_.addTransactionToVerifiedQueue(hash, item.second);
      } else {
        LOG(log_er_) << "Tx " << item.second->getHash().abridged() << " status was changed from in_queue_unverified to "
                     << static_cast<int>(status.state) << " while it was in unverified queue";
      }
    }
  }
}

void TransactionManager::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

void TransactionManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.start();
  verifiers_.clear();
  for (size_t i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyQueuedTrxs(); });
  }
  assert(num_verifiers_ == verifiers_.size());
}

void TransactionManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.stop();
  for (auto &t : verifiers_) {
    t.join();
  }
}

std::unordered_map<trx_hash_t, Transaction> TransactionManager::getVerifiedTrxSnapShot() const {
  return trx_qu_.getVerifiedTrxSnapShot();
}

std::pair<size_t, size_t> TransactionManager::getTransactionQueueSize() const {
  return trx_qu_.getTransactionQueueSize();
}

size_t TransactionManager::getTransactionBufferSize() const { return trx_qu_.getTransactionBufferSize(); }

std::vector<Transaction> TransactionManager::getNewVerifiedTrxSnapShot() {
  // TODO: get rid of copying the whole vector of txs here...
  return trx_qu_.getNewVerifiedTrxSnapShot();
}

unsigned long TransactionManager::getTransactionCount() const { return trx_count_; }

TransactionQueue &TransactionManager::getTransactionQueue() { return trx_qu_; }

void TransactionManager::addTrxCount(unsigned long num) { trx_count_ += num; }

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> TransactionManager::getTransaction(
    trx_hash_t const &hash) const {
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> tr;
  auto t = trx_qu_.getTransaction(hash);
  if (t) {  // find in queue
    tr = std::make_shared<std::pair<Transaction, taraxa::bytes>>(std::make_pair(*t, *t->rlp()));
  } else {  // search from db
    tr = db_->getTransactionExt(hash);
  }
  return tr;
}

/**
 * This is for block proposer
 * Few steps:
 * 1. get a snapshot (move) of verified queue (lock)
 *	  now, verified trxs can include (A) unpacked ,(B) packed by other ,(C)
 *old trx that only seen in db
 * 2. write A, B to database, of course C will be rejected (already exist in
 *database)
 * 3. propose transactions for block A
 * 4. update A, B and C status to seen_in_db
 */
void TransactionManager::packTrxs(vec_trx_t &to_be_packed_trx, uint16_t max_trx_to_pack) {
  to_be_packed_trx.clear();
  std::list<Transaction> list_trxs;

  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot(max_trx_to_pack);

  auto trx_batch = db_->createWriteBatch();
  vector<h256> accepted_trx_hashes;
  accepted_trx_hashes.reserve(verified_trx.size());
  {
    std::unique_lock transaction_status_lock(transaction_status_mutex_);
    for (auto const &i : verified_trx) {
      trx_hash_t const &hash = i.first;
      Transaction const &trx = i.second;
      auto status = db_->getTransactionStatus(hash);
      if (status.state == TransactionStatusEnum::in_queue_verified) {
        // Skip if transaction is already in existing block
        status.state = TransactionStatusEnum::in_block;
        db_->addTransactionStatusToBatch(trx_batch, hash, status);
        trx_count_.fetch_add(1);
        accepted_trx_hashes.emplace_back(hash);
        LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
        // update transaction_status
        list_trxs.push_back(trx);
      }
    }

    if (!accepted_trx_hashes.empty()) {
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
      for (auto const &h : accepted_trx_hashes) {
        transaction_accepted_.emit(h);
      }
    }
  }

  if (list_trxs.size() == 0) {
    return;
  }

  // sort trx based on sender and nonce
  list_trxs.sort(trxComp);

  std::transform(list_trxs.begin(), list_trxs.end(), std::back_inserter(to_be_packed_trx),
                 [](Transaction const &t) { return t.getHash(); });
}

void TransactionManager::updateFinalizedTransactionsStatus(SyncBlock const &sync_block) {
  // TODO: rework tx_status synchronization...
  std::unique_lock transaction_status_lock(transaction_status_mutex_);
  auto write_batch = db_->createWriteBatch();
  uint64_t position = 0;
  for (auto const &trx : sync_block.transactions) {
    db_->removeTransactionToBatch(trx.getHash(), write_batch);
    db_->addTransactionStatusToBatch(
        write_batch, trx.getHash(),
        TransactionStatus(TransactionStatusEnum::finalized, sync_block.pbft_blk->getPeriod(), position));
    position++;
  }
  db_->commitWriteBatch(write_batch);
}

// Save the transaction that came with the block together with the
// transactions that are in the queue. This will update the transaction
// status as well and remove the transactions from the queue
bool TransactionManager::verifyBlockTransactions(DagBlock const &blk, std::vector<Transaction> const &trxs) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << blk.getHash() << " since it has no transactions";
    return false;
  }

  DbStorage::MultiGetQuery db_query(db_, all_block_trx_hashes.size());
  db_query.append(DbStorage::Columns::trx_status, all_block_trx_hashes);
  auto db_trxs_statuses = db_query.execute(false);

  std::unordered_map<trx_hash_t, Transaction> known_trx(trxs.size());

  std::transform(trxs.begin(), trxs.end(), std::inserter(known_trx, known_trx.end()),
                 [](Transaction const &t) { return std::make_pair(t.getHash(), t); });

  bool all_transactions_saved = true;
  trx_hash_t missing_trx;
  auto trx_batch = db_->createWriteBatch();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); ++idx) {
    TransactionStatus status;
    if (!db_trxs_statuses[idx].empty()) {
      auto data = asBytes(db_trxs_statuses[idx]);
      status = TransactionStatus(RLP(data));
    }

    if (status.state == TransactionStatusEnum::in_queue_unverified || status.state == TransactionStatusEnum::not_seen) {
      const trx_hash_t &trx_hash = all_block_trx_hashes[idx];
      if (known_trx.count(trx_hash)) {
        if (const auto valid = verifyTransaction(known_trx[trx_hash]); !valid.first) {
          LOG(log_er_) << "Block " << blk.getHash() << " has invalid transaction " << trx_hash.toString() << " "
                       << valid.second;
          return false;
        }
        db_->addTransactionToBatch(known_trx[trx_hash], trx_batch, true);

      } else if (status.state == TransactionStatusEnum::in_queue_unverified) {
        auto tx = db_->getTransaction(trx_hash);
        if (const auto valid = verifyTransaction(*tx); !valid.first) {
          LOG(log_er_) << "Block " << blk.getHash() << " has invalid transaction " << trx_hash.toString() << " "
                       << valid.second;
          return false;
        }
        db_->addTransactionToBatch(*tx, trx_batch, true);

      } else {
        all_transactions_saved = false;
        missing_trx = trx_hash;
        break;
      }
    }
  }

  db_->commitWriteBatch(trx_batch);

  if (all_transactions_saved) {
    std::vector<trx_hash_t> accepted_trx_hashes;
    {
      // TODO: rework tx_status synchronization...
      std::unique_lock transaction_status_lock(transaction_status_mutex_);

      size_t newly_added_txs_to_block_counter = 0;
      trx_batch = db_->createWriteBatch();
      accepted_trx_hashes.reserve(all_block_trx_hashes.size());
      db_trxs_statuses = db_query.execute();
      for (size_t idx = 0; idx < db_trxs_statuses.size(); ++idx) {
        TransactionStatus status;
        if (!db_trxs_statuses[idx].empty()) {
          auto data = asBytes(db_trxs_statuses[idx]);
          status = TransactionStatus(RLP(data));
        }

        if (status.state != TransactionStatusEnum::in_block && status.state != TransactionStatusEnum::finalized) {
          const trx_hash_t &trx_hash = all_block_trx_hashes[idx];
          newly_added_txs_to_block_counter++;
          accepted_trx_hashes.push_back(trx_hash);
          db_->addTransactionStatusToBatch(trx_batch, trx_hash, TransactionStatusEnum::in_block);
        }
      }
      // Write prepared batch to db
      trx_count_ += newly_added_txs_to_block_counter;
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count_, trx_batch);

      db_->commitWriteBatch(trx_batch);
    }
    for (auto const &h : accepted_trx_hashes) {
      transaction_accepted_.emit(h);
    }
    trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes);
  } else {
    LOG(log_er_) << "Block " << blk.getHash() << " has missing transaction " << missing_trx;
  }

  return all_transactions_saved;
}

}  // namespace taraxa
