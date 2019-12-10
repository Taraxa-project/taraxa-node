#include "dag_block.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <utility>
#include "dag.hpp"
#include "full_node.hpp"
#include "libdevcore/CommonData.h"
#include "libdevcore/CommonJS.h"
#include "libdevcore/Log.h"

namespace taraxa {

using std::to_string;

DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips,
                   vec_trx_t trxs, sig_t sig, blk_hash_t hash,
                   addr_t sender) try : pivot_(pivot),
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
DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips,
                   vec_trx_t trxs) try : pivot_(pivot),
                                         level_(level),
                                         tips_(tips),
                                         trxs_(trxs) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  assert(false);
}
DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips,
                   vec_trx_t trxs, VdfSortition const &vdf) try
    : DagBlock(pivot, level, tips, trxs) {
  vdf_ = vdf;
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
  assert(false);
}

DagBlock::DagBlock(string const &json) {
  Json::Value doc;
  Json::Reader reader;
  bool parsing_successful = reader.parse(json, doc);
  if (!parsing_successful) {
    std::cerr << "Parsing Json Transaction failed" << json << std::endl;
    assert(false);
  }
  level_ = level_t(doc["level"].asUInt64());
  tips_ = asVector<blk_hash_t>(doc["tips"]);
  trxs_ = asVector<trx_hash_t>(doc["trxs"]);
  sig_ = sig_t(doc["sig"].asString());
  hash_ = blk_hash_t(doc["hash"].asString());
  cached_sender_ = addr_t(doc["sender"].asString());
  pivot_ = blk_hash_t(doc["pivot"].asString());
  timestamp_ = doc["timestamp"].asInt64();
  auto vdf_string = doc["vdf"].asString();
  if (!vdf_string.empty()) {
    vdf_ = VdfSortition(dev::fromHex(vdf_string));
  }
}
DagBlock::DagBlock(boost::property_tree::ptree const &doc) {
  level_ = level_t(doc.get<level_t>("level"));
  tips_ = asVector<blk_hash_t, std::string>(doc, "tips");
  trxs_ = asVector<trx_hash_t, std::string>(doc, "trxs");
  sig_ = sig_t(doc.get<std::string>("sig"));
  hash_ = blk_hash_t(doc.get<std::string>("hash"));
  cached_sender_ = addr_t(doc.get<std::string>("sender"));
  pivot_ = blk_hash_t(doc.get<std::string>("pivot"));
  timestamp_ = doc.get<int64_t>("timestamp");
  auto vdf_string = doc.get<std::string>("vdf");
  if (!vdf_string.empty()) {
    vdf_ = VdfSortition(dev::fromHex(vdf_string));
  }
}
DagBlock::DagBlock(bytes const &_rlp) {
  dev::RLP const rlp(_rlp);
  if (!rlp.isList())
    throw std::invalid_argument("transaction RLP must be a list");

  hash_ = rlp[0].toHash<blk_hash_t>();
  pivot_ = rlp[1].toHash<blk_hash_t>();
  level_ = rlp[2].toInt<level_t>();
  timestamp_ = rlp[3].toInt<int64_t>();
  vdf_ = vdf_sortition::VdfSortition(rlp[4].toBytes());
  uint64_t num_tips = rlp[5].toInt<uint64_t>();
  for (auto i = 0; i < num_tips; ++i) {
    auto tip = rlp[6 + i].toHash<blk_hash_t>();
    tips_.push_back(tip);
  }
  uint64_t num_trxs = rlp[6 + num_tips].toInt<uint64_t>();
  for (auto i = 0; i < num_trxs; ++i) {
    auto trx = rlp[7 + num_tips + i].toHash<trx_hash_t>();
    trxs_.push_back(trx);
  }
  if (rlp.itemCount() > 7 + num_tips + num_trxs)
    sig_ = rlp[7 + num_tips + num_trxs].toHash<sig_t>();
}

bool DagBlock::isValid() const {
  return !(pivot_.isZero() && hash_.isZero() && sig_.isZero() &&
           cached_sender_.isZero());
}

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
  res["sender"] = dev::toJS(cached_sender_);
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
    cached_sender_ =
        dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}

void DagBlock::streamRLP(dev::RLPStream &s, bool include_sig) const {
  auto num_tips = tips_.size();
  auto num_trxs = trxs_.size();
  auto total = num_tips + num_trxs + 7;
  s.appendList(include_sig ? total + 1 : total);
  s << hash_;
  s << pivot_;
  s << level_;
  s << timestamp_;
  s << vdf_.rlp();
  s << num_tips;
  for (auto i = 0; i < num_tips; ++i) s << tips_[i];
  s << num_trxs;
  for (auto i = 0; i < num_trxs; ++i) s << trxs_[i];
  if (include_sig) {
    s << sig_;
  }
}

bytes DagBlock::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig);
  return s.out();
}

blk_hash_t DagBlock::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

BlockManager::BlockManager(size_t capacity, unsigned num_verifiers)
    : capacity_(capacity),
      num_verifiers_(num_verifiers),
      blk_status_(10000, 100),
      seen_blocks_(10000, 100) {}

BlockManager::~BlockManager() { stop(); }

void BlockManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Create verifier threads = " << num_verifiers_ << std::endl;
  trx_mgr_ = node_.lock()->getTransactionManager();
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
  return seen_blocks_.count(hash);
}

std::shared_ptr<DagBlock> BlockManager::getDagBlock(
    blk_hash_t const &hash) const {
  std::shared_ptr<DagBlock> ret;
  auto blk = seen_blocks_.get(hash);
  if (blk.second) {
    ret = std::make_shared<DagBlock>(blk.first);
  }
  return ret;
}

void BlockManager::pushUnverifiedBlock(
    DagBlock const &blk, std::vector<Transaction> const &transactions,
    bool critical) {
  seen_blocks_.update(blk.getHash(), blk);
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    if (critical) {
      blk_status_.insert(blk.getHash(), BlockStatus::proposed);

      unverified_qu_[blk.getLevel()].emplace_front(
          std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from front: " << blk.getHash()
                   << std::endl;
    } else {
      blk_status_.insert(blk.getHash(), BlockStatus::broadcasted);
      unverified_qu_[blk.getLevel()].emplace_back(
          std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from back: " << blk.getHash()
                   << std::endl;
    }
  }
  cond_for_unverified_qu_.notify_one();
}

void BlockManager::pushUnverifiedBlock(DagBlock const &blk, bool critical) {
  pushUnverifiedBlock(blk, std::vector<Transaction>(), critical);
}

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
  if (verified_qu_.begin()->second.empty())
    verified_qu_.erase(verified_qu_.begin());
  return blk;
}

void BlockManager::pushVerifiedBlock(DagBlock const &blk) {
  uLock lock(shared_mutex_for_verified_qu_);
  verified_qu_[blk.getLevel()].emplace_back(blk);
}

void BlockManager::verifyBlock() {
  auto log_time = node_.lock()->getTimeLogger();
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
      if (unverified_qu_.begin()->second.empty())
        unverified_qu_.erase(unverified_qu_.begin());
    }
    auto status = blk_status_.get(blk.first.getHash());
    // Verifying transaction ...
    LOG(log_time) << "VerifyingTrx block  " << blk.first.getHash()
                  << " at: " << getCurrentTimeMilliSeconds();
    // only need to verify if this is a broadcasted block (proposed block are
    // generated by verified trx)
    if (!status.second || status.first == BlockStatus::broadcasted) {
      bool valid = trx_mgr_->verifyBlockTransactions(blk.first, blk.second);
      if (!valid) {
        LOG(log_er_) << "Ignore block " << blk.first.getHash()
                     << " since it has invalid or missing transactions";
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

    LOG(log_time) << "VerifiedTrx stored " << blk.first.getHash()
                  << " at: " << getCurrentTimeMilliSeconds();

    cond_for_verified_qu_.notify_one();
    LOG(log_dg_) << "Verified block: " << blk.first.getHash() << std::endl;
  }
}

}  // namespace taraxa
