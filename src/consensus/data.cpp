#include "data.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa {
using namespace dev;
using namespace std;

VrfPbftSortition::VrfPbftSortition(bytes const& b) {
  dev::RLP const rlp(b);
  if (!rlp.isList()) throw std::invalid_argument("VrfPbftSortition RLP must be a list");
  pk = rlp[0].toHash<vrf_pk_t>();
  pbft_msg.blk = rlp[1].toHash<blk_hash_t>();
  pbft_msg.type = PbftVoteTypes(rlp[2].toInt<uint>());
  pbft_msg.round = rlp[3].toInt<uint64_t>();
  pbft_msg.step = rlp[4].toInt<size_t>();
  proof = rlp[5].toHash<vrf_proof_t>();
  verify();
}
bytes VrfPbftSortition::getRlpBytes() const {
  dev::RLPStream s;
  s.appendList(6);
  s << pk;
  s << pbft_msg.blk;
  s << pbft_msg.type;
  s << pbft_msg.round;
  s << pbft_msg.step;
  s << proof;
  return s.out();
}

/*
 * Sortition return true:
 * CREDENTIAL / SIGNATURE_HASH_MAX <= SORTITION THRESHOLD / VALID PLAYERS
 * i.e., CREDENTIAL * VALID PLAYERS <= SORTITION THRESHOLD * SIGNATURE_HASH_MAX
 * otherwise return false
 */
bool VrfPbftSortition::canSpeak(size_t threshold, size_t valid_players) const {
  uint1024_t left = (uint1024_t)((uint512_t)output) * valid_players;
  uint1024_t right = (uint1024_t)max512bits * threshold;
  return left <= right;
}

Vote::Vote(dev::RLP const& rlp) {
  if (!rlp.isList()) throw std::invalid_argument("vote RLP must be a list");
  blockhash_ = rlp[0].toHash<blk_hash_t>();
  vrf_sortition_ = VrfPbftSortition(rlp[1].toBytes());
  vote_signatue_ = rlp[2].toHash<sig_t>();
  vote_hash_ = sha3(true);
}

Vote::Vote(bytes const& b) : Vote(dev::RLP(b)) {}

Vote::Vote(secret_t const& node_sk, VrfPbftSortition const& vrf_sortition, blk_hash_t const& blockhash)
    : vrf_sortition_(vrf_sortition), blockhash_(blockhash) {
  vote_signatue_ = dev::sign(node_sk, sha3(false));
  vote_hash_ = sha3(true);
}

bytes Vote::rlp(bool inc_sig) const {
  dev::RLPStream s;
  s.appendList(inc_sig ? 3 : 2);

  s << blockhash_;
  s << vrf_sortition_.getRlpBytes();
  if (inc_sig) {
    s << vote_signatue_;
  }

  return s.out();
}

void Vote::voter() const {
  if (cached_voter_) return;
  cached_voter_ = dev::recover(vote_signatue_, sha3(false));
  assert(cached_voter_);
}

PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& r) {
  dev::RLP const rlp(r);
  if (!rlp.isList()) throw std::invalid_argument("PBFT RLP must be a list");
  prev_block_hash_ = rlp[0].toHash<blk_hash_t>();
  dag_block_hash_as_pivot_ = rlp[1].toHash<blk_hash_t>();
  period_ = rlp[2].toInt<uint64_t>();
  timestamp_ = rlp[3].toInt<uint64_t>();
  signature_ = rlp[4].toHash<sig_t>();
  calculateHash_();
}

PbftBlock::PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot, uint64_t period,
                     addr_t const& beneficiary, secret_t const& sk)
    : prev_block_hash_(prev_blk_hash),
      dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
      period_(period),
      beneficiary_(beneficiary) {
  timestamp_ = dev::utcTime();
  signature_ = dev::sign(sk, sha3(false));
  calculateHash_();
}

PbftBlock::PbftBlock(std::string const& str) {
  Json::Value doc;
  Json::Reader reader;
  reader.parse(str, doc);
  block_hash_ = blk_hash_t(doc["block_hash"].asString());
  prev_block_hash_ = blk_hash_t(doc["prev_block_hash"].asString());
  dag_block_hash_as_pivot_ = blk_hash_t(doc["dag_block_hash_as_pivot"].asString());
  period_ = doc["period"].asUInt64();
  timestamp_ = doc["timestamp"].asUInt64();
  signature_ = sig_t(doc["signature"].asString());
  beneficiary_ = addr_t(doc["beneficiary"].asString());
}

Json::Value PbftBlock::toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks) {
  auto ret = b.getJson();
  // Legacy schema
  auto& schedule_json = ret["schedule"] = Json::Value(Json::objectValue);
  auto& dag_blks_json = schedule_json["dag_blocks_order"] = Json::Value(Json::arrayValue);
  for (auto const& h : dag_blks) {
    dag_blks_json.append(dev::toJS(h));
  }
  return ret;
}

void PbftBlock::calculateHash_() {
  if (!block_hash_) {
    block_hash_ = dev::sha3(rlp(true));
  } else {
    // Hash sould only be calculated once
    assert(false);
  }
  auto p = dev::recover(signature_, sha3(false));
  assert(p);
  beneficiary_ = dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
}

blk_hash_t PbftBlock::sha3(bool include_sig) const { return dev::sha3(rlp(include_sig)); }

std::string PbftBlock::getJsonStr() const { return getJson().toStyledString(); }

Json::Value PbftBlock::getJson() const {
  Json::Value json;
  json["prev_block_hash"] = prev_block_hash_.toString();
  json["dag_block_hash_as_pivot"] = dag_block_hash_as_pivot_.toString();
  json["period"] = (Json::Value::UInt64)period_;
  json["timestamp"] = (Json::Value::UInt64)timestamp_;
  json["block_hash"] = block_hash_.toString();
  json["signature"] = signature_.toString();
  json["beneficiary"] = beneficiary_.toString();
  return json;
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm, bool include_sig) const {
  strm.appendList(include_sig ? 5 : 4);
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
  strm << period_;
  strm << timestamp_;
  if (include_sig) {
    strm << signature_;
  }
}

bytes PbftBlock::rlp(bool include_sig) const {
  RLPStream strm;
  streamRLP(strm, include_sig);
  return strm.out();
}

std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk) {
  strm << pbft_blk.getJsonStr();
  return strm;
}

PbftBlockCert::PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes)
    : pbft_blk(new PbftBlock(pbft_blk)), cert_votes(cert_votes) {}

PbftBlockCert::PbftBlockCert(dev::RLP const& rlp) {
  pbft_blk.reset(new PbftBlock(rlp[0]));
  for (auto const& el : rlp[1]) {
    cert_votes.emplace_back(el);
  }
}

PbftBlockCert::PbftBlockCert(bytes const& all_rlp) : PbftBlockCert(dev::RLP(all_rlp)) {}

bytes PbftBlockCert::rlp() const {
  RLPStream s(2);
  s.appendRaw(pbft_blk->rlp(true));
  s.appendList(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.appendRaw(v.rlp());
  }
  return s.out();
}

void PbftBlockCert::encode_raw(RLPStream& rlp, PbftBlock const& pbft_blk, dev::bytesConstRef votes_raw) {
  rlp.appendList(2).appendRaw(pbft_blk.rlp(true)).appendRaw(votes_raw);
}

std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b) {
  strm << "[PbftBlockCert] : " << b.pbft_blk << " , num of votes " << b.cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa