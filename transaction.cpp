#include "transaction.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <utility>
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
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyQueuedTrxs(); });
  }
  assert(num_verifiers_ == verifiers_.size());
}

void TransactionQueue::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cond_for_unverified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool TransactionQueue::insert(Transaction const &trx, bool critical) {
  trx_hash_t hash = trx.getHash();
  auto status = trx_status_.get(hash);
  bool ret = false;
  listIter iter;

  if (status.second == false) {  // never seen before
    ret = trx_status_.insert(hash, TransactionStatus::in_queue_unverified);
    if (ret) {
      {
        uLock lock(shared_mutex_for_queued_trxs_);
        iter = trx_buffer_.insert(trx_buffer_.end(), trx);
        assert(iter != trx_buffer_.end());
        queued_trxs_[trx.getHash()] = iter;
      }
      {
        uLock lock(shared_mutex_for_unverified_qu_);
        if (critical) {
          unverified_hash_qu_.emplace_front(std::make_pair(hash, iter));
        } else {
          unverified_hash_qu_.emplace_back(std::make_pair(hash, iter));
        }
      }
      cond_for_unverified_qu_.notify_one();
      LOG(log_nf_) << getFullNodeAddress() << " Trx: " << hash << " inserted. "
                   << std::endl;
    } else {
      // If ret is false status was just changed by another thread so ask for it
      // again
      status = trx_status_.get(hash);
    }
  }
  return ret;
}

void TransactionQueue::verifyQueuedTrxs() {
  while (!stopped_) {
    // Transaction utrx;
    std::pair<trx_hash_t, listIter> item;
    {
      uLock lock(shared_mutex_for_unverified_qu_);
      while (unverified_hash_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) {
        LOG(log_nf_) << "Transaction verifier stopped ... " << std::endl;
        return;
      }
      item = unverified_hash_qu_.front();
      unverified_hash_qu_.pop_front();
    }
    try {
      trx_hash_t hash = item.first;
      Transaction trx = *(item.second);
      // verify and put the transaction to verified queue
      bool valid = false;
      if (mode_ == VerifyMode::skip_verify_sig) {
        valid = true;
      } else {
        valid = trx.verifySig();
      }
      // mark invalid
      if (!valid) {
        trx_status_.update(trx.getHash(), TransactionStatus::invalid);
        {
          uLock lock(shared_mutex_for_queued_trxs_);
          trx_buffer_.erase(queued_trxs_[hash]);
          queued_trxs_.erase(hash);
        }
        LOG(log_wr_) << getFullNodeAddress() << " Trx: " << hash << "invalid. "
                     << std::endl;
        continue;
      }
      bool ret = trx_status_.update(trx.getHash(),
                                    TransactionStatus::in_queue_verified,
                                    TransactionStatus::in_queue_unverified);
      if (ret) {
        {
          uLock lock(shared_mutex_for_verified_qu_);
          verified_trxs_[hash] = item.second;
          new_verified_transactions_ = true;
        }
      }

    } catch (...) {
      assert(false);
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
TransactionQueue::moveVerifiedTrxSnapShot() {
  std::unordered_map<trx_hash_t, Transaction> res;
  {
    uLock lock(shared_mutex_for_verified_qu_);
    for (auto const &trx : verified_trxs_) {
      res[trx.first] = *(trx.second);
    }
    verified_trxs_.clear();
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

void TransactionManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  if (!full_node_.lock()) {
    LOG(log_wr_) << "FullNode is not set ...";
    assert(db_trxs_);
  } else {
    assert(!db_trxs_);
    auto full_node = full_node_.lock();
    assert(full_node);
    db_trxs_ = full_node->getTrxsDB();
    DagBlock blk;
    string pivot;
    std::vector<std::string> tips;
    full_node->getLatestPivotAndTips(pivot, tips);
    DagFrontier frontier;
    frontier.pivot = blk_hash_t(pivot);
    updateNonce(blk, frontier);
  }
  trx_qu_.start();
}

void TransactionManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  db_trxs_ = nullptr;
  trx_qu_.stop();
}

std::unordered_map<trx_hash_t, Transaction>
TransactionManager::getVerifiedTrxSnapShot() {
  return trx_qu_.getVerifiedTrxSnapShot();
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

unsigned long TransactionManager::getTransactionStatusCount() const {
  return trx_status_.size();
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
      auto trx_bytes = db_trxs_->lookup(hash);
      if (trx_bytes.size() > 0) {
        tr = std::make_shared<std::pair<Transaction, taraxa::bytes>>(trx_bytes,
                                                                     trx_bytes);
        break;
      }
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
    vec_trx_t const &all_block_trx_hashes,
    std::vector<Transaction> const &some_trxs) {
  if (all_block_trx_hashes.empty()) {
    return true;
  }
  // First step: Save and update status for transactions that were sent
  // together with the block some_trxs might not full trxs in the block (for
  // syncing purpose)
  if (!some_trxs.empty()) {
    for (auto const &trx : some_trxs) {
      auto trx_hash = trx.getHash();
      db_trxs_->insert(trx_hash, trx.rlp(trx.hasSig()));
      trx_status_.update(trx_hash, TransactionStatus::in_block);
    }
  }

  // Second step: Remove from the queue any transaction that is part of the
  // block Verify that all transactions are saved in the database If all
  // transactions are not available within 10 seconds fail the block
  // verification
  bool all_transactions_saved = true;
  unsigned int delay = 0;
  vec_trx_t unsaved_trx;
  while (delay < 10000) {
    // {
    //   auto removed_trx =
    //       trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes);
    //   for (auto const &trx : removed_trx) {
    //     db_trxs_->update(trx.first, trx.second.rlp(trx.second.hasSig()));
    //     trx_status_.update(trx.first, TransactionStatus::in_block);
    //   }
    //   db_trxs_->commit();
    // }
    all_transactions_saved = true;
    unsaved_trx.clear();
    for (auto const &trx : all_block_trx_hashes) {
      auto res = trx_status_.get(trx);
      if (res.second == false || res.first != TransactionStatus::in_block) {
        all_transactions_saved = false;
        unsaved_trx.emplace_back(trx);
      }
    }
    // Only if all transactions are saved in the db can we verify a new block
    if (all_transactions_saved) break;
    // Getting here should be a very rare case where in a racing condition
    // block was processed before the transactions
    thisThreadSleepForMilliSeconds(10);
    delay += 10;
  }
  if (!all_transactions_saved) {
    LOG(log_er_) << "Missing transactions " << unsaved_trx;
  }

  return all_transactions_saved;
}

bool TransactionManager::insertTrx(Transaction const &trx,
                                   taraxa::bytes const &trx_serialized,
                                   bool critical) {
  bool ret = false;
  auto hash = trx.getHash();
  auto status = trx_status_.get(hash);
  if (status.second == false) {  // never seen before
    if (db_trxs_->exists(hash)) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in db " << std::endl;
    } else {
      if (trx_qu_.insert(trx, critical)) {
        rlp_cache_.insert(trx.getHash(), trx_serialized);
        ret = true;
        auto node = full_node_.lock();
        if (node) {
          node->newPendingTransaction(trx.getHash());
        }
      }
    }
  } else {
    if (status.first == TransactionStatus::in_queue_unverified) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue (unverified). "
                   << std::endl;
    } else if (status.first == TransactionStatus::in_queue_verified) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue (verified). "
                   << std::endl;
    } else if (status.first == TransactionStatus::in_block) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
    } else if (status.first == TransactionStatus::invalid) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. "
                   << std::endl;
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
                                  DagFrontier &frontier) {
  to_be_packed_trx.clear();

  std::list<Transaction> list_trxs;
  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot();

  bool changed = false;
  auto trx_batch = db_trxs_->createWriteBatch();
  for (auto const &i : verified_trx) {
    trx_hash_t const &hash = i.first;
    Transaction const &trx = i.second;
    auto [rlp, exist1] = rlp_cache_.get(hash);
    // Skip if transaction is already in existing block
    if (db_trxs_->exists(hash)) {
      continue;
    }
    trx_batch->insert(
        db_trxs_->toSlice(hash.asBytes()),
        exist1 ? db_trxs_->toSlice(rlp) : db_trxs_->toSlice(trx.rlp(true)));
    changed = true;

    auto [status, exist2] = trx_status_.get(hash);
    LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
    // update transaction_status
    trx_status_.update(hash, TransactionStatus::in_block);
    list_trxs.push_back(trx);
  }

  if (changed) {
    db_trxs_->commit(std::move(trx_batch));
  }

  // check requeued trx
  if (!trx_requeued_.empty()) {
    LOG(log_dg_) << getFullNodeAddress() << " Number of repacked trx "
                 << trx_requeued_.size();
  }
  while (!trx_requeued_.empty()) {
    list_trxs.push_back(trx_requeued_.front());
    trx_requeued_.pop();
  }

  if (list_trxs.size() == 0) {
    return;
  }

  // sort trx based on sender and nonce
  list_trxs.sort(trxComp);
  // {
  //   std::stringstream all_trxs;
  //   for (auto const &l : list_trxs) {
  //     all_trxs << "[ " << l.getSender().abridged() << " , " << l.getNonce()
  //              << " ] ";
  //   }
  //   LOG(log_si_) << getFullNodeAddress()
  //                << " pre-filtered trxs: " << all_trxs.str();
  // }
  auto orig_size = list_trxs.size();
  // auto iter = list_trxs.begin();
  // // filter out nonce gaps (drop transctions that have gap)
  // int outdated_trx = 0;
  // int gapped_trx = 0;
  // {
  //   uLock lock(mu_for_nonce_table_);
  //   while (iter != list_trxs.end()) {
  //     auto curr_sender = iter->getSender();
  //     auto curr_nonce = iter->getNonce();

  //     auto [curr_sender_prev_nonce, exist] = accs_nonce_.get(curr_sender);

  //     // skip if outdated
  //     if (exist && curr_sender_prev_nonce >= curr_nonce) {
  //       LOG(log_dg_) << getFullNodeAddress() << " Remove trx "
  //                    << iter->getHash() << "Sender " << curr_sender << "
  //                    Nonce "
  //                    << curr_nonce
  //                    << " too old, because prev sender nonce on file is "
  //                    << curr_sender_prev_nonce;
  //       iter = list_trxs.erase(iter);
  //       outdated_trx++;
  //       continue;
  //     }

  //     auto prev_iter = std::prev(iter);
  //     bool is_first_account_seq =
  //         (iter == list_trxs.begin() || prev_iter->getSender() !=
  //         curr_sender);

  //     if (is_first_account_seq) {
  //       if (!exist) {
  //         if (curr_nonce != 0) {
  //           LOG(log_nf_) << getFullNodeAddress() << " Remove trx "
  //                        << iter->getHash() << " Sender " << curr_sender
  //                        << " Nonce " << curr_nonce
  //                        << " cannot be packed, no nonce 0 seen ";
  //           trx_requeued_.push(*iter);
  //           iter = list_trxs.erase(iter);
  //           gapped_trx++;
  //           continue;
  //         }
  //       } else {
  //         if (curr_nonce != curr_sender_prev_nonce + 1) {
  //           LOG(log_nf_) << getFullNodeAddress() << " Remove trx "
  //                        << iter->getHash() << " Sender " << curr_sender
  //                        << " Nonce " << curr_nonce
  //                        << " cannot be packed, no previous nonce available "
  //                        << curr_sender_prev_nonce;
  //           trx_requeued_.push(*iter);
  //           iter = list_trxs.erase(iter);
  //           gapped_trx++;
  //           continue;
  //         }
  //       }
  //     } else {
  //       auto prev_nonce = prev_iter->getNonce();

  //       if (curr_nonce != (prev_nonce + 1)) {
  //         LOG(log_nf_) << getFullNodeAddress() << " Remove trx "
  //                      << iter->getHash() << "Sender " << curr_sender
  //                      << " Nonce " << curr_nonce << " because prev nonce is
  //                      "
  //                      << prev_nonce << " nonce table is "
  //                      << accs_nonce_.get(curr_sender).first;
  //         trx_requeued_.push(*iter);
  //         iter = list_trxs.erase(iter);
  //         gapped_trx++;
  //         continue;
  //       }
  //     }

  //     iter++;
  //   }
  // }
  // if (list_trxs.empty()) {
  //   return;
  // }

  // auto pruned_size = list_trxs.size();
  // if (orig_size != pruned_size) {
  //   LOG(log_dg_) << getFullNodeAddress() << " Shorten trx pack from "
  //                << orig_size << " to " << pruned_size << " outdated "
  //                << outdated_trx << " gapped " << gapped_trx;
  // }

  for (auto const &t : list_trxs) {
    to_be_packed_trx.emplace_back(t.getHash());
  }

  frontier = dag_frontier_;
  LOG(log_dg_) << getFullNodeAddress()
               << " Get frontier with pivot: " << frontier.pivot
               << " tips: " << frontier.tips;

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
  // debug nonce
  // std::map<addr_t, val_t> begin_nonce;
  // std::map<addr_t, val_t> end_nonce;
  // {
  //   for (auto iter = list_trxs.begin(); iter != list_trxs.end(); iter++) {
  //     auto sender = iter->getSender();
  //     auto nonce = iter->getNonce();
  //     if (!begin_nonce.count(sender)) {
  //       begin_nonce[sender] = nonce;
  //     }
  //   }

  //   for (auto iter = list_trxs.rbegin(); iter != list_trxs.rend(); iter++) {
  //     auto sender = iter->getSender();
  //     auto nonce = iter->getNonce();
  //     if (!end_nonce.count(sender)) {
  //       end_nonce[sender] = nonce;
  //     }
  //   }
  // }
  // std::stringstream nonce_range;
  // for (auto iter = begin_nonce.begin(); iter != begin_nonce.end(); iter++) {
  //   auto sender = iter->first;
  //   nonce_range << "Sender " << sender << " : " << begin_nonce[sender] << "
  //   -> "
  //               << end_nonce[sender] << " | ";
  // }
  // LOG(log_dg_) << getFullNodeAddress() << " " << nonce_range.str();
}

bool TransactionManager::verifyBlockTransactions(
    DagBlock const &blk, std::vector<Transaction> const &trxs) {
  bool invalidTransaction = false;
  for (auto const &trx : trxs) {
    auto valid = trx.verifySig();
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
  bool transactionsSave =
      saveBlockTransactionAndDeduplicate(blk.getTrxs(), trxs);
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
