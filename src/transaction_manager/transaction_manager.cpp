#include "transaction_manager.hpp"

#include <libethcore/Exceptions.h>

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
                                       logger::Logger log_time)
    : db_(db), conf_(conf), trx_qu_(node_addr), node_addr_(node_addr), log_time_(log_time) {
  LOG_OBJECTS_CREATE("TRXMGR");
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  trx_count_.store(trx_count);
}

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

std::pair<bool, std::string> TransactionManager::insertTransaction(Transaction const &trx, bool verify) {
  auto ret = insertTrx(trx, verify);
  if (auto net = network_.lock(); net && ret.first && conf_.network.network_transaction_interval == 0) {
    net->onNewTransactions({*trx.rlp()});
  }
  return ret;
}

uint32_t TransactionManager::insertBroadcastedTransactions(
    // transactions coming from broadcastin is less critical
    std::vector<taraxa::bytes> const &transactions) {
  if (stopped_) {
    return 0;
  }
  uint32_t new_trx_count = 0;
  for (auto const &t : transactions) {
    Transaction trx(t);
    if (insertTrx(trx, false).first) new_trx_count++;
    LOG(log_time_) << "Transaction " << trx.getHash() << " brkreceived at: " << getCurrentTimeMilliSeconds();
  }
  return new_trx_count;
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
    // mark invalid
    if (!valid.first) {
      db_->saveTransactionStatus(hash, TransactionStatus::invalid);
      trx_qu_.removeTransactionFromBuffer(hash);

      LOG(log_wr_) << " Trx: " << hash << "invalid: " << valid.second << std::endl;
      continue;
    }
    {
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_unverified) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);

        trx_qu_.addTransactionToVerifiedQueue(hash, item.second);
      }
    }
  }
}

void TransactionManager::setNetwork(std::weak_ptr<Network> network) { network_ = move(network); }

void TransactionManager::setWsServer(std::shared_ptr<WSServer> ws_server) { ws_server_ = ws_server; }

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

std::vector<taraxa::bytes> TransactionManager::getNewVerifiedTrxSnapShotSerialized() {
  auto verified_trxs = trx_qu_.getNewVerifiedTrxSnapShot();
  std::vector<Transaction> vec_trxs;
  std::copy(verified_trxs.begin(), verified_trxs.end(), std::back_inserter(vec_trxs));
  sort(vec_trxs.begin(), vec_trxs.end(), trxComp);
  std::vector<taraxa::bytes> ret;
  for (auto const &t : vec_trxs) {
    ret.emplace_back(*t.rlp());
  }
  return ret;
}

unsigned long TransactionManager::getTransactionCount() const { return trx_count_.load(); }

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

// Received block means some trx might be packed by others
bool TransactionManager::saveBlockTransactionAndDeduplicate(DagBlock const &blk,
                                                            std::vector<Transaction> const &some_trxs) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    return true;
  }
  std::set<trx_hash_t> known_trx_hashes(all_block_trx_hashes.begin(), all_block_trx_hashes.end());

  if (!some_trxs.empty()) {
    auto trx_batch = db_->createWriteBatch();
    for (auto const &trx : some_trxs) {
      db_->addTransactionToBatch(trx, trx_batch);
      known_trx_hashes.erase(trx.getHash());
    }
    db_->commitWriteBatch(trx_batch);
  }

  bool all_transactions_saved = true;
  trx_hash_t missing_trx;
  for (auto const &trx : known_trx_hashes) {
    auto status = db_->getTransactionStatus(trx);
    if (status == TransactionStatus::not_seen) {
      all_transactions_saved = false;
      missing_trx = trx;
      break;
    }
  }

  if (all_transactions_saved) {
    auto trx_batch = db_->createWriteBatch();

    for (auto const &trx : all_block_trx_hashes) {
      auto status = db_->getTransactionStatus(trx);
      if (status != TransactionStatus::in_block) {
        if (status == TransactionStatus::in_queue_unverified) {
          auto valid = verifyTransaction(db_->getTransactionExt(trx)->first);
          if (!valid.first) {
                LOG(log_er_) << " Block contains invalid transaction " << trx << " " << valid.second;
            return false;
          }
        }

        trx_count_.fetch_add(1);
        db_->addTransactionStatusToBatch(trx_batch, trx, TransactionStatus::in_block);
      }
    }

    // Write prepared batch to db
    auto trx_count = trx_count_.load();
    db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);

    db_->commitWriteBatch(trx_batch);
  } else {
        LOG(log_er_) << " Missing transaction - FAILED block verification " << missing_trx;
  }

  if (all_transactions_saved) trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes);

  return all_transactions_saved;
}

std::pair<bool, std::string> TransactionManager::insertTrx(Transaction const &trx, bool verify) {
  auto hash = trx.getHash();
  db_->saveTransaction(trx);

  if (conf_.test_params.max_transaction_queue_warn > 0 || conf_.test_params.max_transaction_queue_drop > 0) {
    auto queue_size = trx_qu_.getTransactionQueueSize();
    if (conf_.test_params.max_transaction_queue_drop > 0 &&
        conf_.test_params.max_transaction_queue_drop <= queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Trx: " << hash << "skipped, queue too large. Unverified queue: " << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.test_params.max_transaction_queue_drop;
      return std::make_pair(false, "Queue overlfow");
    } else if (conf_.test_params.max_transaction_queue_warn > 0 &&
               conf_.test_params.max_transaction_queue_warn <= queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Warning: queue large. Unverified queue: " << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.test_params.max_transaction_queue_drop;
    }
  }

  std::pair<bool, std::string> verified;
  verified.first = true;
  if (verify && mode_ != VerifyMode::skip_verify_sig) {
    verified = verifyTransaction(trx);
  }

  if (verified.first) {
    auto status = db_->getTransactionStatus(hash);
    if (status == TransactionStatus::not_seen) {
      if (verify) {
        status = TransactionStatus::in_queue_verified;
      } else {
        status = TransactionStatus::in_queue_unverified;
      }
      db_->saveTransactionStatus(hash, status);

      trx_qu_.insert(trx, verify);
      if (ws_server_) ws_server_->newPendingTransaction(trx.getHash());
      return std::make_pair(true, "");
    } else {
      switch (status) {
        case TransactionStatus::in_queue_verified:
          LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
          return std::make_pair(false, "in verified queue");
        case TransactionStatus::in_queue_unverified:
          LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
          return std::make_pair(false, "in unverified queue");
        case TransactionStatus::in_block:
          LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
          return std::make_pair(false, "in block");
        case TransactionStatus::invalid:
          LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. " << std::endl;
          return std::make_pair(false, "already invalid");
        default:
          return std::make_pair(false, "unknown");
      }
    }
  }
  return verified;
}  // namespace taraxa

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

  bool changed = false;
  auto trx_batch = db_->createWriteBatch();
  {
    for (auto const &i : verified_trx) {
      trx_hash_t const &hash = i.first;
      Transaction const &trx = i.second;
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_verified) {
        // Skip if transaction is already in existing block
        db_->addTransactionStatusToBatch(trx_batch, hash, TransactionStatus::in_block);
        trx_count_.fetch_add(1);
        changed = true;
        LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
        // update transaction_status
        list_trxs.push_back(trx);
      }
    }

    if (changed) {
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
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

bool TransactionManager::verifyBlockTransactions(DagBlock const &blk, std::vector<Transaction> const &trxs) {
  bool invalidTransaction = false;
  for (auto const &trx : trxs) {
    auto valid = verifyTransaction(trx);
    if (!valid.first) {
      invalidTransaction = true;
      LOG(log_er_) << "Invalid transaction " << trx.getHash().toString() << " " << valid.second;
    }
  }
  if (invalidTransaction) {
    return false;
  }

  // Save the transaction that came with the block together with the
  // transactions that are in the queue. This will update the transaction
  // status as well and remove the transactions from the queue
  bool transactionsSave = saveBlockTransactionAndDeduplicate(blk, trxs);
  if (!transactionsSave) {
    LOG(log_er_) << "Block " << blk.getHash() << " has missing transactions ";
    return false;
  }
  return true;
}

}  // namespace taraxa
