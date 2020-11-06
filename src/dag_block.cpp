#include "dag_block.hpp"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include <utility>

#include "dag.hpp"
#include "transaction_manager.hpp"

namespace taraxa {

using std::to_string;

DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t sig, blk_hash_t hash, addr_t sender) try
    : pivot_(pivot),
      level_(level),
      tips_(tips),
      trxs_(trxs),
      sig_(sig),
      hash_(hash),
      cached_sender_(sender) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  assert(false);
}
DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs) try : pivot_(pivot), level_(level), tips_(tips), trxs_(trxs) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  assert(false);
}
DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, VdfSortition const &vdf) try
    : DagBlock(pivot, level, tips, trxs) {
  vdf_ = vdf;
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  assert(false);
}

DagBlock::DagBlock(string const &json)
    : DagBlock([&] {
        Json::Value doc;
        stringstream(json) >> doc;
        return doc;
      }()) {}

DagBlock::DagBlock(Json::Value const &doc) {
  if (auto const &v = doc["level"]; v.isString()) {
    level_ = dev::jsToInt(v.asString());
  } else {
    // this was inconsistent with getJson()
    // fixme: eliminate this branch
    level_ = v.asUInt64();
  }
  tips_ = asVector<blk_hash_t, std::string>(doc, "tips");
  trxs_ = asVector<trx_hash_t, std::string>(doc, "trxs");
  sig_ = sig_t(doc["sig"].asString());
  hash_ = blk_hash_t(doc["hash"].asString());
  cached_sender_ = addr_t(doc["sender"].asString());
  pivot_ = blk_hash_t(doc["pivot"].asString());
  if (auto const &v = doc["timestamp"]; v.isString()) {
    timestamp_ = dev::jsToInt(v.asString());
  } else {
    // this was inconsistent with getJson()
    // fixme: eliminate this branch
    timestamp_ = v.asUInt64();
  }

  auto vdf_string = doc["vdf"].asString();
  if (!vdf_string.empty()) {
    vdf_ = VdfSortition(cached_sender_, dev::fromHex(vdf_string));
  }
}

DagBlock::DagBlock(dev::RLP const &rlp) {
  if (!rlp.isList()) {
    throw std::invalid_argument("transaction RLP must be a list");
  }
  uint field_n = 0;
  for (auto const &el : rlp) {
    if (field_n == 0) {
      pivot_ = el.toHash<blk_hash_t>();
    } else if (field_n == 1) {
      level_ = el.toInt<level_t>();
    } else if (field_n == 2) {
      timestamp_ = el.toInt<uint64_t>();
    } else if (field_n == 3) {
      vdf_ = vdf_sortition::VdfSortition(cached_sender_, el.toBytes());
    } else if (field_n == 4) {
      tips_ = el.toVector<trx_hash_t>();
    } else if (field_n == 5) {
      trxs_ = el.toVector<trx_hash_t>();
    } else if (field_n == 6) {
      sig_ = el.toHash<sig_t>();
    } else {
      BOOST_THROW_EXCEPTION(std::runtime_error("too many rlp fields for dag block"));
    }
    ++field_n;
  }
  updateHash();
}

std::vector<trx_hash_t> DagBlock::extract_transactions_from_rlp(RLP const &rlp) { return rlp[5].toVector<trx_hash_t>(); }

bool DagBlock::isValid() const { return !(pivot_.isZero() && hash_.isZero() && sig_.isZero() && cached_sender_.isZero()); }

Json::Value DagBlock::getJson() const {
  Json::Value res;
  res["pivot"] = dev::toJS(pivot_);
  res["level"] = dev::toJS(level_);
  res["tips"] = Json::Value(Json::arrayValue);
  for (auto const &t : tips_) {
    res["tips"].append(dev::toJS(t));
  }
  res["transactions"] = Json::Value(Json::arrayValue);
  for (auto const &t : trxs_) {
    res["transactions"].append(dev::toJS(t));
  }
  res["sig"] = dev::toJS(sig_);
  res["hash"] = dev::toJS(hash_);
  res["sender"] = dev::toJS(sender());
  res["timestamp"] = dev::toJS(timestamp_);
  res["vdf"] = dev::toJS(dev::toHex(vdf_.rlp()));
  return res;
}

std::string DagBlock::getJsonStr() const {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, getJson());
}

void DagBlock::sign(secret_t const &sk) {
  if (!sig_) {
    timestamp_ = dev::utcTime();
    sig_ = dev::sign(sk, sha3(false));
  }
  sender();
  updateHash();
}

bool DagBlock::verifySig() const {
  if (!sig_) return false;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);
  return dev::verify(pk, sig_, msg);
}

addr_t DagBlock::sender() const {
  if (!cached_sender_) {
    if (!sig_) {
      return addr_t{};
    }
    auto p = dev::recover(sig_, sha3(false));
    assert(p);
    cached_sender_ = dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}

void DagBlock::streamRLP(dev::RLPStream &s, bool include_sig) const {
  constexpr auto base_field_count = 6;
  s.appendList(include_sig ? base_field_count + 1 : base_field_count);
  s << pivot_;
  s << level_;
  s << timestamp_;
  s << vdf_.rlp();
  s.appendVector(tips_);
  s.appendVector(trxs_);
  if (include_sig) {
    s << sig_;
  }
}

bytes DagBlock::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig);
  return s.out();
}

blk_hash_t DagBlock::sha3(bool include_sig) const { return dev::sha3(rlp(include_sig)); }

BlockManager::BlockManager(size_t capacity, unsigned num_verifiers, addr_t node_addr, std::shared_ptr<DbStorage> db,
                           std::shared_ptr<TransactionManager> trx_mgr, boost::log::sources::severity_channel_logger<> log_time, uint32_t queue_limit)
    : capacity_(capacity),
      num_verifiers_(num_verifiers),
      blk_status_(10000, 100),
      seen_blocks_(10000, 100),
      queue_limit_(queue_limit),
      db_(db),
      trx_mgr_(trx_mgr),
      log_time_(log_time) {
  LOG_OBJECTS_CREATE("BLKQU");
}

BlockManager::~BlockManager() { stop(); }

void BlockManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Create verifier threads = " << num_verifiers_ << std::endl;
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
    verifiers_.emplace_back([this, i]() { this->verifyBlock(); });
  }
}

void BlockManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cond_for_unverified_qu_.notify_all();
  cond_for_verified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool BlockManager::isBlockKnown(blk_hash_t const &hash) {
  auto known = seen_blocks_.count(hash);
  if (!known) return getDagBlock(hash) != nullptr;
  return true;
}

std::shared_ptr<DagBlock> BlockManager::getDagBlock(blk_hash_t const &hash) const {
  std::shared_ptr<DagBlock> ret;
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    ret = std::make_shared<DagBlock>(blk.first);
  }
  if (!ret) {
    return db_->getDagBlock(hash);
  }
  return ret;
}

bool BlockManager::pivotAndTipsValid(DagBlock const &blk) {
  auto status = blk_status_.get(blk.getPivot());
  if (status.second && status.first == BlockStatus::invalid) {
    blk_status_.update(blk.getHash(), BlockStatus::invalid);
    LOG(log_dg_) << "DAG Block " << blk.getHash() << " pivot " << blk.getPivot() << " unavailable";
    return false;
  }
  for (auto const &t : blk.getTips()) {
    auto status = blk_status_.get(t);
    if (status.second && status.first == BlockStatus::invalid) {
      blk_status_.update(blk.getHash(), BlockStatus::invalid);
      LOG(log_dg_) << "DAG Block " << blk.getHash() << " tip " << t << " unavailable";
      return false;
    }
  }
  return true;
}

level_t BlockManager::getMaxDagLevelInQueue() const {
  level_t max_level = 0;
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    if (unverified_qu_.size() != 0) max_level = unverified_qu_.rbegin()->first;
  }
  {
    uLock lock(shared_mutex_for_verified_qu_);
    if (verified_qu_.size() != 0) max_level = std::max(verified_qu_.rbegin()->first, max_level);
  }
  return max_level;
}

void BlockManager::insertBlock(DagBlock const &blk) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_nf_) << "Block known " << blk.getHash();
    return;
  }
  pushUnverifiedBlock(std::move(blk), true /*critical*/);
  LOG(log_time_) << "Store cblock " << blk.getHash() << " at: " << getCurrentTimeMilliSeconds() << " ,trxs: " << blk.getTrxs().size()
                 << " , tips: " << blk.getTips().size();
}

void BlockManager::pushUnverifiedBlock(DagBlock const &blk, std::vector<Transaction> const &transactions, bool critical) {
  if (queue_limit_ > 0) {
    auto queue_size = getDagBlockQueueSize();
    if (queue_limit_ > queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Warning: block queue large. Unverified queue: " << queue_size.first << "; Verified queue: " << queue_size.second
                   << "; Limit: " << queue_limit_;
    }
  }
  seen_blocks_.update(blk.getHash(), blk);
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    if (critical) {
      blk_status_.insert(blk.getHash(), BlockStatus::proposed);

      unverified_qu_[blk.getLevel()].emplace_front(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from front: " << blk.getHash() << std::endl;
    } else {
      blk_status_.insert(blk.getHash(), BlockStatus::broadcasted);
      unverified_qu_[blk.getLevel()].emplace_back(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from back: " << blk.getHash() << std::endl;
    }
  }
  cond_for_unverified_qu_.notify_one();
}

void BlockManager::insertBroadcastedBlockWithTransactions(DagBlock const &blk, std::vector<Transaction> const &transactions) {
  if (isBlockKnown(blk.getHash())) {
    LOG(log_dg_) << "Block known " << blk.getHash();
    return;
  }
  pushUnverifiedBlock(blk, transactions, false /*critical*/);
  LOG(log_time_) << "Store ncblock " << blk.getHash() << " at: " << getCurrentTimeMilliSeconds() << " ,trxs: " << blk.getTrxs().size()
                 << " , tips: " << blk.getTips().size();
}

void BlockManager::pushUnverifiedBlock(DagBlock const &blk, bool critical) { pushUnverifiedBlock(blk, std::vector<Transaction>(), critical); }

std::pair<size_t, size_t> BlockManager::getDagBlockQueueSize() const {
  std::pair<size_t, size_t> res;
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    res.first = 0;
    for (auto &it : unverified_qu_) {
      res.first += it.second.size();
    }
  }

  {
    uLock lock(shared_mutex_for_verified_qu_);
    res.second = 0;
    for (auto &it : verified_qu_) {
      res.second += it.second.size();
    }
  }
  return res;
}

DagBlock BlockManager::popVerifiedBlock() {
  uLock lock(shared_mutex_for_verified_qu_);
  while (verified_qu_.empty() && !stopped_) {
    cond_for_verified_qu_.wait(lock);
  }
  if (stopped_) return DagBlock();

  auto blk = verified_qu_.begin()->second.front();
  verified_qu_.begin()->second.pop_front();
  if (verified_qu_.begin()->second.empty()) verified_qu_.erase(verified_qu_.begin());
  return blk;
}

void BlockManager::pushVerifiedBlock(DagBlock const &blk) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_qu_[blk.getLevel()].emplace_back(blk);
}

void BlockManager::verifyBlock() {
  while (!stopped_) {
    std::pair<DagBlock, std::vector<Transaction>> blk;
    {
      uLock lock(shared_mutex_for_unverified_qu_);
      while (unverified_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) return;
      blk = unverified_qu_.begin()->second.front();
      unverified_qu_.begin()->second.pop_front();
      if (unverified_qu_.begin()->second.empty()) unverified_qu_.erase(unverified_qu_.begin());
    }
    auto status = blk_status_.get(blk.first.getHash());
    // Verifying transaction ...
    LOG(log_time_) << "VerifyingTrx block  " << blk.first.getHash() << " at: " << getCurrentTimeMilliSeconds();
    // only need to verify if this is a broadcasted block (proposed block are
    // generated by verified trx)
    if (!status.second || status.first == BlockStatus::broadcasted) {
      bool valid = trx_mgr_->verifyBlockTransactions(blk.first, blk.second);
      if (valid) {
        // Verify VDF solution
        vdf_sortition::VdfSortition vdf = blk.first.getVdf();
        if (!vdf.verifyVdf(blk.first.getLevel(), blk.first.getPivot().toString())) {
          LOG(log_er_) << "DAG block " << blk.first.getHash() << " failed on VDF verification with pivot hash " << blk.first.getPivot();
          valid = false;
        }
      }
      if (!valid) {
        LOG(log_er_) << "Ignore block " << blk.first.getHash() << " since it has invalid or missing transactions";
        blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        continue;
      }
    }

    {
      uLock lock(shared_mutex_for_verified_qu_);
      if (status.second && status.first == BlockStatus::proposed) {
        verified_qu_[blk.first.getLevel()].emplace_front(blk.first);
      } else if (!status.second || status.first == BlockStatus::broadcasted) {
        verified_qu_[blk.first.getLevel()].emplace_back(blk.first);
      }
    }
    blk_status_.update(blk.first.getHash(), BlockStatus::verified);

    LOG(log_time_) << "VerifiedTrx stored " << blk.first.getHash() << " at: " << getCurrentTimeMilliSeconds();

    cond_for_verified_qu_.notify_one();
    LOG(log_dg_) << "Verified block: " << blk.first.getHash() << std::endl;
  }
}

}  // namespace taraxa
