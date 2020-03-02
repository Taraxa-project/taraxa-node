#include "transaction.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <utility>

#include "eth/util.hpp"
#include "full_node.hpp"

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

Transaction::Transaction(stream &strm) { deserialize(strm); }

Transaction::Transaction(string const &json) {
  try {
    Json::Value root;
    Json::Reader reader;
    bool parsing_successful = reader.parse(json, root);
    if (!parsing_successful) {
      std::cerr << "Parsing Json Transaction failed" << json << std::endl;
      assert(false);
    }

    hash_ = trx_hash_t(root["hash"].asString());
    type_ = toEnum<Transaction::Type>(root["type"].asUInt());
    nonce_ = val_t(root["nonce"].asString());
    value_ = val_t(root["value"].asString());
    gas_price_ = val_t(root["gas_price"].asString());
    gas_ = val_t(root["gas"].asString());
    sig_ = sig_t(root["sig"].asString());
    receiver_ = addr_t(root["receiver"].asString());
    string data = root["data"].asString();
    data_ = dev::jsToBytes(data);
    if (!sig_.isZero()) {
      dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
      if (sig_struct.isValid()) {
        vrs_ = sig_struct;
      }
    }
    chain_id_ = root["chain_id"].asInt();
    ;
    cached_sender_ = addr_t(root["hash"].asString());
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    assert(false);
  }
}

Transaction::Transaction(bytes const &_rlp) {
  dev::RLP const rlp(_rlp);
  if (!rlp.isList())
    throw std::invalid_argument("transaction RLP must be a list");

  nonce_ = rlp[0].toInt<dev::u256>();
  gas_price_ = rlp[1].toInt<dev::u256>();
  gas_ = rlp[2].toInt<dev::u256>();
  type_ = rlp[3].isEmpty() ? taraxa::Transaction::Type::Create
                           : taraxa::Transaction::Type::Call;
  receiver_ = rlp[3].isEmpty()
                  ? dev::Address()
                  : rlp[3].toHash<dev::Address>(dev::RLP::VeryStrict);
  value_ = rlp[4].toInt<dev::u256>();

  if (!rlp[5].isData())
    throw std::invalid_argument("transaction data RLP must be an array");

  data_ = rlp[5].toBytes();

  int const v = rlp[6].toInt<int>();
  dev::h256 const r = rlp[7].toInt<dev::u256>();
  dev::h256 const s = rlp[8].toInt<dev::u256>();

  if (!r && !s) {
    chain_id_ = v;
    sig_ = dev::SignatureStruct{r, s, 0};
  } else {
    if (v > 36)
      chain_id_ = (v - 35) / 2;
    else if (v == 27 || v == 28)
      chain_id_ = -4;
    else
      throw std::invalid_argument("InvalidSignature()");

    sig_ = dev::SignatureStruct{r, s,
                                static_cast<::byte>(v - (chain_id_ * 2 + 35))};
  }
  dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
  if (sig_struct.isValid()) {
    vrs_ = sig_struct;
  }

  if (rlp.itemCount() > 9)
    throw std::invalid_argument("too many fields in the transaction RLP");
  updateHash();
}

bool Transaction::serialize(stream &strm) const {
  bool ok = true;
  ok &= write(strm, hash_);
  ok &= write(strm, type_);
  ok &= write(strm, nonce_);
  ok &= write(strm, value_);
  ok &= write(strm, gas_price_);
  ok &= write(strm, gas_);
  ok &= write(strm, receiver_);
  ok &= write(strm, sig_);
  std::size_t byte_size = data_.size();
  ok &= write(strm, byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= write(strm, data_[i]);
  }
  assert(ok);
  return ok;
}

bool Transaction::deserialize(stream &strm) {
  bool ok = true;
  ok &= read(strm, hash_);
  ok &= read(strm, type_);
  ok &= read(strm, nonce_);
  ok &= read(strm, value_);
  ok &= read(strm, gas_price_);
  ok &= read(strm, gas_);
  ok &= read(strm, receiver_);
  ok &= read(strm, sig_);
  std::size_t byte_size;
  ok &= read(strm, byte_size);
  data_.resize(byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= read(strm, data_[i]);
  }
  if (!sig_.isZero()) {
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }

  assert(ok);
  return ok;
}

Json::Value Transaction::getJson() const {
  Json::Value res;
  res["hash"] = dev::toJS(hash_);
  res["type"] = (uint)type_;
  res["nonce"] = dev::toJS(nonce_);
  res["value"] = dev::toJS(value_);
  res["gas_price"] = dev::toJS(gas_price_);
  res["gas"] = dev::toJS(gas_);
  res["sig"] = dev::toJS(sig_);
  res["receiver"] = dev::toJS(receiver_);
  res["data"] = dev::toJS(data_);
  res["chain_id"] = chain_id_;
  res["sender"] = dev::toJS(cached_sender_);
  return res;
}

string Transaction::getJsonStr() const {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, getJson());
}

void Transaction::sign(secret_t const &sk) {
  if (!sig_) {
    sig_ = dev::sign(sk, sha3(false));
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }
  sender();
  updateHash();
}

bool Transaction::verifySig() const {
  if (!sig_) return false;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);
  // offload send() computation to verifier
  sender();
  return dev::verify(pk, sig_, msg);
}

bool TransactionManager::verifyTransaction(Transaction const &trx) const {
  try {
    if (eth_service_) {
      eth_service_->sealEngine()->verifyTransaction(
          dev::eth::ImportRequirements::Everything,
          eth::util::trx_taraxa_2_eth(trx),  //
          eth_service_->head(),              //
          0);
    } else {
      return trx.verifySig();
    }
    return true;
  } catch (...) {  // Once we are sure what exception can be thrown we should
                   // only catch those exceptions
  }
  return false;
}

addr_t Transaction::sender() const {
  if (!cached_sender_) {
    if (!sig_) {
      return addr_t{};
    }
    auto p = dev::recover(sig_, sha3(false));
    assert(p);
    cached_sender_ =
        dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}
void Transaction::streamRLP(dev::RLPStream &s, bool include_sig,
                            bool _forEip155hash) const {
  if (type_ == Transaction::Type::Null) return;
  s.appendList(include_sig || _forEip155hash ? 9 : 6);
  s << nonce_ << gas_price_ << gas_;
  if (type_ == Transaction::Type::Call) {
    s << receiver_;
  } else {
    s << "";
  }
  s << value_ << data_;
  if (include_sig) {
    assert(vrs_);
    if (hasZeroSig()) {
      s << chain_id_;
    } else {
      int const v_offset = chain_id_ * 2 + 35;
      s << (vrs_->v + v_offset);
    }
    s << (dev::u256)vrs_->r << (dev::u256)vrs_->s;
  } else if (_forEip155hash)
    s << chain_id_ << 0 << 0;
}

bytes Transaction::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig, chain_id_ > 0 && !include_sig);
  return s.out();
};

blk_hash_t Transaction::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

void TransactionQueue::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
}

void TransactionQueue::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cond_for_unverified_qu_.notify_all();
}

void TransactionQueue::insert(Transaction const &trx, bool verify) {
  trx_hash_t hash = trx.getHash();
  listIter iter;

  {
    uLock lock(shared_mutex_for_queued_trxs_);
    iter = trx_buffer_.insert(trx_buffer_.end(), trx);
    assert(iter != trx_buffer_.end());
    queued_trxs_[trx.getHash()] = iter;
  }
  if (verify) {
    uLock lock(shared_mutex_for_verified_qu_);
    verified_trxs_[trx.getHash()] = iter;
    new_verified_transactions_ = true;
  } else {
    uLock lock(shared_mutex_for_unverified_qu_);
    unverified_hash_qu_.emplace_back(std::make_pair(hash, iter));
    cond_for_unverified_qu_.notify_one();
  }
  LOG(log_nf_) << getFullNodeAddress() << " Trx: " << hash << " inserted. "
               << verify << std::endl;
}

std::pair<trx_hash_t, TransactionQueue::listIter>
TransactionQueue::getUnverifiedTransaction() {
  std::pair<trx_hash_t, listIter> item;
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    while (unverified_hash_qu_.empty() && !stopped_) {
      cond_for_unverified_qu_.wait(lock);
    }
    if (stopped_) {
      LOG(log_nf_) << "Transaction verifier stopped ... " << std::endl;
    } else {
      item = unverified_hash_qu_.front();
      unverified_hash_qu_.pop_front();
    }
  }
  return item;
}

void TransactionQueue::removeTransactionFromBuffer(trx_hash_t const &hash) {
  uLock lock(shared_mutex_for_queued_trxs_);
  trx_buffer_.erase(queued_trxs_[hash]);
  queued_trxs_.erase(hash);
}

void TransactionQueue::addTransactionToVerifiedQueue(
    trx_hash_t const &hash, std::list<Transaction>::iterator iter) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_trxs_[hash] = iter;
  new_verified_transactions_ = true;
}

void TransactionManager::verifyQueuedTrxs() {
  while (!stopped_) {
    // It will wait if no transaction in unverified queue
    auto item = trx_qu_.getUnverifiedTransaction();
    if (stopped_) return;

    bool valid = false;
    trx_hash_t hash = item.second->getHash();
    // verify and put the transaction to verified queue
    if (mode_ == VerifyMode::skip_verify_sig) {
      valid = true;
    } else {
      valid = verifyTransaction(*item.second);
    }
    // mark invalid
    if (!valid) {
      {
        uLock lock(mu_for_transactions_);
        db_->saveTransactionStatus(hash, TransactionStatus::invalid);
      }
      trx_qu_.removeTransactionFromBuffer(hash);
      LOG(log_wr_) << getFullNodeAddress() << " Trx: " << hash << "invalid. "
                   << std::endl;
      continue;
    }
    {
      uLock lock(mu_for_transactions_);
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_unverified) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);
        lock.unlock();
        trx_qu_.addTransactionToVerifiedQueue(hash, item.second);
      }
    }
  }
}

// The caller is responsible for storing the transaction to db!
std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::removeBlockTransactionsFromQueue(
    vec_trx_t const &all_block_trxs) {
  std::unordered_map<trx_hash_t, Transaction> result;
  {
    std::vector<trx_hash_t> removed_trx;
    for (auto const &trx : all_block_trxs) {
      {
        upgradableLock lock(shared_mutex_for_verified_qu_);
        auto vrf_trx = verified_trxs_.find(trx);
        if (vrf_trx != verified_trxs_.end()) {
          result[vrf_trx->first] = *(vrf_trx->second);
          removed_trx.emplace_back(trx);
          {
            upgradeLock locked(lock);
            verified_trxs_.erase(trx);
          }
        }
      }
    }
    // TODO: can also remove from unverified_hash_qu_

    // clear trx_buffer
    {
      uLock lock(shared_mutex_for_queued_trxs_);
      for (auto const &t : removed_trx) {
        trx_buffer_.erase(queued_trxs_[t]);
        queued_trxs_.erase(t);
      }
    }
  }
  return result;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::getVerifiedTrxSnapShot() {
  std::unordered_map<trx_hash_t, Transaction> verified_trxs;
  sharedLock lock(shared_mutex_for_verified_qu_);
  for (auto const &trx : verified_trxs_) {
    verified_trxs[trx.first] = *(trx.second);
  }
  LOG(log_dg_) << "Get: " << verified_trxs.size() << " verified trx out. "
               << std::endl;
  return verified_trxs;
}

std::vector<Transaction> TransactionQueue::getNewVerifiedTrxSnapShot() {
  std::vector<Transaction> verified_trxs;
  if (new_verified_transactions_) {
    new_verified_transactions_ = false;
    sharedLock lock(shared_mutex_for_verified_qu_);
    for (auto const &trx : verified_trxs_) {
      verified_trxs.emplace_back(*(trx.second));
    }
    LOG(log_dg_) << "Get: " << verified_trxs.size()
                 << "verified trx out for gossiping " << std::endl;
  }
  return verified_trxs;
}

// search from queued_trx_
std::shared_ptr<Transaction> TransactionQueue::getTransaction(
    trx_hash_t const &hash) const {
  {
    sharedLock lock(shared_mutex_for_queued_trxs_);
    auto it = queued_trxs_.find(hash);
    if (it != queued_trxs_.end()) {
      return std::make_shared<Transaction>(*(it->second));
    }
  }
  return nullptr;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::moveVerifiedTrxSnapShot(uint16_t max_trx_to_pack) {
  std::unordered_map<trx_hash_t, Transaction> res;
  {
    uLock lock(shared_mutex_for_verified_qu_);
    if (max_trx_to_pack == 0) {
      for (auto const &trx : verified_trxs_) {
        res[trx.first] = *(trx.second);
      }
      verified_trxs_.clear();
    } else {
      auto it = verified_trxs_.begin();
      uint16_t counter = 0;
      while (it != verified_trxs_.end() && max_trx_to_pack != counter) {
        res[it->first] = *(it->second);
        it = verified_trxs_.erase(it);
        counter++;
      }
    }
  }
  {
    uLock lock(shared_mutex_for_queued_trxs_);
    for (auto const &t : res) {
      trx_buffer_.erase(queued_trxs_[t.first]);
      queued_trxs_.erase(t.first);
    }
  }
  if (res.size() > 0) {
    LOG(log_dg_) << "Copy " << res.size() << " verified trx. " << std::endl;
  }
  return std::move(res);
}

unsigned long TransactionQueue::getVerifiedTrxCount() {
  sharedLock lock(shared_mutex_for_verified_qu_);
  return verified_trxs_.size();
}

addr_t TransactionQueue::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

void TransactionManager::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  db_ = full_node->getDB();
  trx_qu_.setFullNode(full_node);
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  trx_count_.store(trx_count);
  DagBlock blk;
  string pivot;
  std::vector<std::string> tips;
  full_node->getLatestPivotAndTips(pivot, tips);
  DagFrontier frontier;
  frontier.pivot = blk_hash_t(pivot);
  updateNonce(blk, frontier);
}

void TransactionManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.start();
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
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

std::unordered_map<trx_hash_t, Transaction>
TransactionManager::getVerifiedTrxSnapShot() {
  return trx_qu_.getVerifiedTrxSnapShot();
}

std::pair<size_t, size_t> TransactionManager::getTransactionQueueSize() const {
  return trx_qu_.getTransactionQueueSize();
}

std::pair<size_t, size_t> TransactionQueue::getTransactionQueueSize() const {
  std::pair<size_t, size_t> res;
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    res.first = unverified_hash_qu_.size();
  }
  {
    uLock lock(shared_mutex_for_verified_qu_);
    res.second = verified_trxs_.size();
  }
  return res;
}

std::vector<taraxa::bytes>
TransactionManager::getNewVerifiedTrxSnapShotSerialized() {
  auto verified_trxs = trx_qu_.getNewVerifiedTrxSnapShot();
  std::vector<Transaction> vec_trxs;
  for (auto const &t : verified_trxs) vec_trxs.emplace_back(t);
  sort(vec_trxs.begin(), vec_trxs.end(), trxComp);
  std::vector<taraxa::bytes> ret;
  for (auto const &t : vec_trxs) {
    auto [trx_rlp, exist] = rlp_cache_.get(t.getHash());
    // if cached miss, compute on the fly
    ret.emplace_back(exist ? trx_rlp : t.rlp(true));
  }
  return ret;
}

unsigned long TransactionManager::getTransactionCount() const {
  return trx_count_.load();
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>>
TransactionManager::getTransaction(trx_hash_t const &hash) const {
  // Check the status
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> tr;
  // Loop needed because moving transactions from queue to database is not
  // secure Probably a better fix is to have transactions saved to the
  // database first and only then removed from the queue
  uint counter = 0;
  while (tr == nullptr) {
    auto t = trx_qu_.getTransaction(hash);
    if (t) {  // find in queue
      auto [trx_rlp, exist] = rlp_cache_.get(t->getHash());
      if (!exist) {
        LOG(log_dg_) << "Cannot find rlp in cache ";
      }
      tr = std::make_shared<std::pair<Transaction, taraxa::bytes>>(
          std::make_pair(*t, exist ? trx_rlp : t->rlp(true)));
    } else {  // search from db
      tr = db_->getTransactionExt(hash);
    }
    thisThreadSleepForMilliSeconds(1);
    counter++;
    if (counter % 10000 == 0) {
      LOG(log_wr_) << "Keep waiting transaction " << hash;
    }
  }
  return tr;
}
// Received block means some trx might be packed by others
bool TransactionManager::saveBlockTransactionAndDeduplicate(
    DagBlock const &blk, std::vector<Transaction> const &some_trxs) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    return true;
  }
  uint32_t saved_trx_count = 0;
  std::set<trx_hash_t> known_trx_hashes(all_block_trx_hashes.begin(),
                                        all_block_trx_hashes.end());

  if (!some_trxs.empty()) {
    auto trx_batch = db_->createWriteBatch();
    for (auto const &trx : some_trxs) {
      db_->addTransactionToBatch(trx, trx_batch);
      known_trx_hashes.erase(trx.getHash());
    }
    db_->commitWriteBatch(trx_batch);
  }

  unsigned int delay = 0;
  bool all_transactions_saved = true;
  trx_hash_t missing_trx;
  while (delay < 10000) {
    all_transactions_saved = true;
    for (auto const &trx : known_trx_hashes) {
      auto status = db_->getTransactionStatus(trx);
      if (status == TransactionStatus::not_seen) {
        all_transactions_saved = false;
        missing_trx = trx;
        break;
      }
    }
    // Only if all transactions are saved in the db can we verify a new block
    if (all_transactions_saved) break;
    // Since networking messages are processes on a single thread it should be
    // impossible to have any missing transactions, for now log an error and
    // wait for it but we really should never get here for a valid block
    LOG(log_er_)
        << "Missing transactions that should have been already received";
    thisThreadSleepForMilliSeconds(10);
    delay += 10;
  }
  if (all_transactions_saved) {
    uLock lock(mu_for_transactions_);
    auto trx_batch = db_->createWriteBatch();
    for (auto const &trx : all_block_trx_hashes) {
      auto status = db_->getTransactionStatus(trx);
      if (status != TransactionStatus::in_block) {
        if (status == TransactionStatus::in_queue_unverified) {
          if (!verifyTransaction(db_->getTransactionExt(trx)->first)) {
            LOG(log_er_) << full_node_.lock()->getAddress()
                         << " Block contains invalid transaction " << trx;
            return false;
          }
        }
        trx_count_.fetch_add(1);
        db_->addTransactionStatusToBatch(trx_batch, trx,
                                         TransactionStatus::in_block);
      }
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
    }
  } else {
    LOG(log_er_) << full_node_.lock()->getAddress() << " Missing transaction "
                 << missing_trx;
  }

  return all_transactions_saved;
}

bool TransactionManager::insertTrx(Transaction const &trx,
                                   taraxa::bytes const &trx_serialized,
                                   bool verify) {
  bool verified = false;
  auto hash = trx.getHash();
  db_->saveTransaction(trx);
  bool ret = false;

  if (conf_.max_transaction_queue_warn > 0 ||
      conf_.max_transaction_queue_drop > 0) {
    auto queue_size = trx_qu_.getTransactionQueueSize();
    if (conf_.max_transaction_queue_drop >
        queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Trx: " << hash
                   << "skipped, queue too large. Unverified queue: "
                   << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.max_transaction_queue_drop;
      return false;
    } else if (conf_.max_transaction_queue_warn >
               queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Warning: queue large. Unverified queue: "
                   << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.max_transaction_queue_drop;
      return false;
    }
  }

  if (verify) {
    verified = (mode_ == VerifyMode::skip_verify_sig) || verifyTransaction(trx);
  }
  if (!verify || verified) {
    uLock lock(mu_for_transactions_);
    auto status = db_->getTransactionStatus(hash);
    if (status == TransactionStatus::not_seen) {
      if (verify) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);
      } else {
        db_->saveTransactionStatus(hash,
                                   TransactionStatus::in_queue_unverified);
      }
      lock.unlock();
      trx_qu_.insert(trx, verify);
      auto node = full_node_.lock();
      if (node) {
        node->newPendingTransaction(trx.getHash());
      }
      ret = true;
    } else {
      if (status == TransactionStatus::in_queue_verified ||
          status == TransactionStatus::in_queue_unverified) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
      } else if (status == TransactionStatus::in_block) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
      } else if (status == TransactionStatus::invalid) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. "
                     << std::endl;
      }
    }
  }
  return ret;
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
void TransactionManager::packTrxs(vec_trx_t &to_be_packed_trx,
                                  DagFrontier &frontier,
                                  uint16_t max_trx_to_pack) {
  to_be_packed_trx.clear();
  std::list<Transaction> list_trxs;

  frontier = dag_frontier_;
  LOG(log_dg_) << getFullNodeAddress()
               << " Get frontier with pivot: " << frontier.pivot
               << " tips: " << frontier.tips;

  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot(max_trx_to_pack);

  bool changed = false;
  auto trx_batch = db_->createWriteBatch();
  std::vector<trx_hash_t> trxs_written_to_db;
  {
    uLock lock(mu_for_transactions_);
    for (auto const &i : verified_trx) {
      trx_hash_t const &hash = i.first;
      Transaction const &trx = i.second;
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_verified) {
        // Skip if transaction is already in existing block
        db_->addTransactionStatusToBatch(trx_batch, hash,
                                         TransactionStatus::in_block);
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
  auto orig_size = list_trxs.size();

  for (auto const &t : list_trxs) {
    to_be_packed_trx.emplace_back(t.getHash());
  }

  auto full_node = full_node_.lock();
  if (full_node) {
    // Need to update pivot incase a new period is confirmed
    std::vector<std::string> ghost;
    full_node->getGhostPath(ghost);
    vec_blk_t gg;
    for (auto const &t : ghost) {
      gg.emplace_back(blk_hash_t(t));
    }
    for (auto const &g : gg) {
      if (g == frontier.pivot) {  // pivot does not change
        break;
      }
      for (auto &t : frontier.tips) {
        if (g == t) {
          std::swap(frontier.pivot, t);
          LOG(log_si_) << getFullNodeAddress()
                       << " Swap frontier with pivot: " << dag_frontier_.pivot
                       << " tips: " << frontier.pivot;
          break;
        }
      }
    }
  }
}

bool TransactionManager::verifyBlockTransactions(
    DagBlock const &blk, std::vector<Transaction> const &trxs) {
  bool invalidTransaction = false;
  for (auto const &trx : trxs) {
    auto valid = verifyTransaction(trx);
    if (!valid) {
      invalidTransaction = true;
      LOG(log_er_) << "Invalid transaction " << trx.getHash().toString();
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

void TransactionManager::updateNonce(DagBlock const &blk,
                                     DagFrontier const &frontier) {
  auto full_node = full_node_.lock();
  if (!full_node) {
    return;
  }
  uLock lock(mu_for_nonce_table_);
  for (auto const &t : blk.getTrxs()) {
    auto trx = getTransaction(t);
    assert(trx);
    auto trx_sender = trx->first.getSender();
    auto trx_hash = trx->first.getHash();
    auto [prev_nonce, exist] = accs_nonce_.get(trx_sender);
    auto trx_nonce = trx->first.getNonce();
    auto new_nonce = trx_nonce > prev_nonce ? trx_nonce : prev_nonce;
    accs_nonce_.update(trx_sender, new_nonce);
  }

  dag_frontier_ = frontier;
  LOG(log_dg_) << getFullNodeAddress() << " Update nonce of block "
               << blk.getHash() << " frontier: " << frontier.pivot
               << " tips: " << frontier.tips
               << " dag_frontier: " << dag_frontier_.pivot
               << " dag_tips: " << dag_frontier_.tips;
}

addr_t TransactionManager::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

}  // namespace taraxa
