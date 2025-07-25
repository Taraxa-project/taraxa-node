#include "dag/dag_block.hpp"

#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

#include <utility>

#include "common/encoding_rlp.hpp"
#include "common/util.hpp"

namespace taraxa {

using vrf_wrapper::VrfSortitionBase;

DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, sig_t sig,
                   blk_hash_t hash, addr_t sender)
    : pivot_(std::move(pivot)),
      level_(level),
      tips_(std::move(tips)),
      trxs_(std::move(trxs)),
      gas_estimation_(std::move(est)),
      sig_(std::move(sig)),
      hash_(std::move(hash)),
      cached_sender_(std::move(sender)) {}

DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash,
                   addr_t sender)
    : DagBlock(std::move(pivot), level, std::move(tips), std::move(trxs), {}, std::move(signature), std::move(hash),
               std::move(sender)) {}

DagBlock::DagBlock(blk_hash_t const &pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, secret_t const &sk)
    : DagBlock(pivot, level, std::move(tips), std::move(trxs), {}, VdfSortition(), sk) {}

DagBlock::DagBlock(blk_hash_t const &pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est,
                   VdfSortition vdf, secret_t const &sk)
    : pivot_(pivot),
      level_(level),
      tips_(std::move(tips)),
      trxs_(std::move(trxs)),
      gas_estimation_(std::move(est)),
      timestamp_(dev::utcTime()),
      vdf_(std::move(vdf)) {
  sig_ = dev::sign(sk, sha3(false));
}

DagBlock::DagBlock(std::string const &json)
    : DagBlock([&] {
        Json::Value doc;
        std::stringstream(json) >> doc;
        return doc;
      }()) {}

DagBlock::DagBlock(Json::Value const &doc) {
  level_ = dev::getUInt(doc["level"]);

  tips_ = asVector<blk_hash_t>(doc["tips"]);
  trxs_ = asVector<trx_hash_t>(doc["trxs"]);
  gas_estimation_ = doc["gas_estimation"].asUInt64();
  sig_ = sig_t(doc["sig"].asString());
  pivot_ = blk_hash_t(doc["pivot"].asString());
  timestamp_ = dev::getUInt(doc["timestamp"]);

  // Allow vdf not to be present for genesis
  if (level_ > 0) {
    vdf_ = VdfSortition(doc["vdf"]);
  }
}

DagBlock::DagBlock(dev::RLP const &rlp) {
  dev::bytes vdf_bytes;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pivot_, level_, timestamp_, vdf_bytes, tips_, trxs_, sig_,
                  gas_estimation_);
  vdf_ = vdf_sortition::VdfSortition(vdf_bytes);
}

DagBlock::DagBlock(dev::RLP const &rlp, vec_trx_t &&trxs) {
  dev::bytes vdf_bytes;
  util::rlp_tuple(util::RLPDecoderRef(rlp, true), pivot_, level_, timestamp_, vdf_bytes, tips_, sig_, gas_estimation_);
  vdf_ = vdf_sortition::VdfSortition(vdf_bytes);
  trxs_ = std::move(trxs);
}

level_t DagBlock::extract_dag_level_from_rlp(const dev::RLP &rlp) { return rlp[kLevelPosInRlp].toInt<level_t>(); }
sig_t DagBlock::extract_signature_from_rlp(const dev::RLP &rlp) { return rlp[kSigPosInRlp].toHash<sig_t>(); }

Json::Value DagBlock::getJson(bool with_derived_fields) const {
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
  res["trx_estimations"] = dev::toJS(gas_estimation_);
  res["sig"] = dev::toJS(sig_);
  if (with_derived_fields) {
    res["hash"] = dev::toJS(getHash());
    res["sender"] = dev::toJS(getSender());
  }
  res["timestamp"] = dev::toJS(timestamp_);
  if (0 < level_) {
    res["vdf"] = vdf_.getJson();
  }
  return res;
}

std::string DagBlock::getJsonStr() const {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, getJson());
}

bool DagBlock::verifySig() const {
  if (!sig_) return false;
  if (cached_sender_) return true;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);  // recover is equal to verify
  return !pk.isZero();
}

void DagBlock::verifyVdf(const SortitionParams &vdf_config, const h256 &proposal_period_hash,
                         const vrf_wrapper::vrf_pk_t &pk, uint64_t vote_count, uint64_t total_vote_count) const {
  dev::RLPStream s;
  s << getPivot();
  for (const auto &trx : getTrxs()) {
    s << trx;
  }
  dev::bytes vdf_msg = s.invalidate();

  vdf_.verifyVdf(vdf_config, VrfSortitionBase::makeVrfInput(getLevel(), proposal_period_hash), pk, vdf_msg, vote_count,
                 total_vote_count);
}

blk_hash_t const &DagBlock::getHash() const {
  if (!hash_) {
    std::unique_lock l(hash_mu_.val, std::try_to_lock);
    if (!l.owns_lock()) {
      l.lock();
      return hash_;
    }
    hash_ = sha3(true);
  };
  return hash_;
}

addr_t const &DagBlock::getSender() const {
  if (!cached_sender_) {
    std::unique_lock l(cached_sender_mu_.val, std::try_to_lock);
    if (!l.owns_lock()) {
      l.lock();
      return cached_sender_;
    }
    if (!sig_) {
      return cached_sender_;
    }
    auto p = dev::recover(sig_, sha3(false));
    assert(p);
    cached_sender_ = dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}

dev::RLPStream DagBlock::streamRLP(bool include_sig, bool include_trxs) const {
  dev::RLPStream s;

  auto base_field_count = 6;
  if (include_sig) {
    base_field_count += 1;
  }
  if (include_trxs) {
    base_field_count += 1;
  }

  s.appendList(base_field_count);
  s << pivot_;
  s << level_;
  s << timestamp_;
  s << vdf_.rlp();
  s.appendVector(tips_);
  if (include_trxs) {
    s.appendVector(trxs_);
  }
  if (include_sig) {
    s << sig_;
  }
  s << gas_estimation_;

  return s;
}

bytes DagBlock::rlp(bool include_sig, bool include_trxs) const {
  return streamRLP(include_sig, include_trxs).invalidate();
}

blk_hash_t DagBlock::sha3(bool include_sig) const { return dev::sha3(rlp(include_sig)); }

void DagBlock::rlp(::taraxa::util::RLPDecoderRef encoding) { *this = DagBlock(encoding.value); }

void DagBlock::rlp(::taraxa::util::RLPEncoderRef encoding) const { encoding.appendRaw(rlp(true)); }

}  // namespace taraxa
