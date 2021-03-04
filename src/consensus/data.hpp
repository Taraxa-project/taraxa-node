#pragma once

#include <json/json.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

#include "common/types.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {

enum PbftMgrRoundStep : uint8_t { PbftRound = 0, PbftStep };
enum PbftMgrStatus {
  soft_voted_block_in_round = 0,
  executed_block,
  executed_in_round,
  cert_voted_in_round,
  next_voted_soft_value,
  next_voted_null_block_hash,
  next_voted_block_in_previous_round
};
enum PbftMgrVotedValue {
  own_starting_value_in_round = 0,
  soft_voted_block_hash_in_round,
  next_voted_block_hash_in_previous_round,
};

enum PbftVoteTypes { propose_vote_type = 0, soft_vote_type, cert_vote_type, next_vote_type };

struct VrfPbftMsg {
  VrfPbftMsg() = default;
  VrfPbftMsg(blk_hash_t const& blk, PbftVoteTypes type, uint64_t round, size_t step)
      : blk(blk), type(type), round(round), step(step) {}
  std::string toString() const {
    return blk.toString() + "_" + std::to_string(type) + "_" + std::to_string(round) + "_" + std::to_string(step);
  }
  bool operator==(VrfPbftMsg const& other) const {
    return blk == other.blk && type == other.type && round == other.round && step == other.step;
  }
  friend std::ostream& operator<<(std::ostream& strm, VrfPbftMsg const& pbft_msg) {
    strm << "  [Vrf Pbft Msg] " << std::endl;
    strm << "    blk_hash: " << pbft_msg.blk << std::endl;
    strm << "    type: " << pbft_msg.type << std::endl;
    strm << "    round: " << pbft_msg.round << std::endl;
    strm << "    step: " << pbft_msg.step << std::endl;
    return strm;
  }

  bytes getRlpBytes() const {
    dev::RLPStream s;
    s.appendList(4);
    s << blk;
    s << type;
    s << round;
    s << step;
    return s.out();
  }

  blk_hash_t blk;
  PbftVoteTypes type;
  uint64_t round;
  size_t step;
};

struct VrfPbftSortition : public vrf_wrapper::VrfSortitionBase {
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using vrf_output_t = vrf_wrapper::vrf_output_t;
  using bytes = dev::bytes;
  VrfPbftSortition() = default;
  VrfPbftSortition(vrf_sk_t const& sk, VrfPbftMsg const& pbft_msg)
      : pbft_msg(pbft_msg), VrfSortitionBase(sk, pbft_msg.getRlpBytes()) {}
  explicit VrfPbftSortition(bytes const& rlp);
  bytes getRlpBytes() const;
  bool verify() { return VrfSortitionBase::verify(pbft_msg.getRlpBytes()); }
  bool operator==(VrfPbftSortition const& other) const {
    return pk == other.pk && pbft_msg == other.pbft_msg && proof == other.proof && output == other.output;
  }
  static inline uint512_t max512bits = std::numeric_limits<uint512_t>::max();
  bool canSpeak(size_t threshold, size_t valid_players) const;
  friend std::ostream& operator<<(std::ostream& strm, VrfPbftSortition const& vrf_sortition) {
    strm << "[VRF sortition] " << std::endl;
    strm << "  pk: " << vrf_sortition.pk << std::endl;
    strm << "  proof: " << vrf_sortition.proof << std::endl;
    strm << "  output: " << vrf_sortition.output << std::endl;
    strm << vrf_sortition.pbft_msg << std::endl;
    return strm;
  }
  VrfPbftMsg pbft_msg;
};

class Vote {
 public:
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  Vote() = default;
  Vote(secret_t const& node_sk, VrfPbftSortition const& vrf_sortition, blk_hash_t const& blockhash);

  explicit Vote(dev::RLP const& rlp);
  explicit Vote(bytes const& rlp);
  bool operator==(Vote const& other) const { return rlp() == other.rlp(); }
  ~Vote() {}

  vote_hash_t getHash() const { return vote_hash_; }
  public_t getVoter() const {
    voter();
    return cached_voter_;
  }
  auto getVrfSortition() const { return vrf_sortition_; }
  auto getSortitionProof() const { return vrf_sortition_.proof; }
  auto getCredential() const { return vrf_sortition_.output; }
  sig_t getVoteSignature() const { return vote_signatue_; }
  blk_hash_t getBlockHash() const { return blockhash_; }
  PbftVoteTypes getType() const { return vrf_sortition_.pbft_msg.type; }
  uint64_t getRound() const { return vrf_sortition_.pbft_msg.round; }
  size_t getStep() const { return vrf_sortition_.pbft_msg.step; }
  bytes rlp(bool inc_sig = true) const;
  bool verifyVote() const {
    auto msg = sha3(false);
    voter();
    return dev::verify(cached_voter_, vote_signatue_, msg);
  }
  bool verifyCanSpeak(size_t threshold, size_t valid_players) const {
    return vrf_sortition_.canSpeak(threshold, valid_players);
  }

  friend std::ostream& operator<<(std::ostream& strm, Vote const& vote) {
    strm << "[Vote] " << std::endl;
    strm << "  vote_hash: " << vote.vote_hash_ << std::endl;
    strm << "  voter: " << vote.getVoter() << std::endl;
    strm << "  vote_signatue: " << vote.vote_signatue_ << std::endl;
    strm << "  blockhash: " << vote.blockhash_ << std::endl;
    strm << "  vrf_sorition: " << vote.vrf_sortition_ << std::endl;
    return strm;
  }

 private:
  blk_hash_t sha3(bool inc_sig) const { return dev::sha3(rlp(inc_sig)); }
  void voter() const;

  vote_hash_t vote_hash_;  // hash of this vote
  blk_hash_t blockhash_;
  sig_t vote_signatue_;
  VrfPbftSortition vrf_sortition_;
  mutable public_t cached_voter_;
};

class PbftBlock {
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  uint64_t period_;  // Block index, PBFT head block is period 0, first PBFT block is period 1
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;

 public:
  PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot, uint64_t period,
            addr_t const& beneficiary, secret_t const& sk);
  explicit PbftBlock(dev::RLP const& r);
  explicit PbftBlock(bytes const& RLP);
  explicit PbftBlock(std::string const& JSON);

  blk_hash_t sha3(bool include_sig) const;
  std::string getJsonStr() const;
  Json::Value getJson() const;
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;
  bytes rlp(bool include_sig) const;

  static Json::Value toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks);

  auto const& getBlockHash() const { return block_hash_; }
  auto const& getPrevBlockHash() const { return prev_block_hash_; }
  auto const& getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }
  auto getPeriod() const { return period_; }
  auto getTimestamp() const { return timestamp_; }
  auto const& getBeneficiary() const { return beneficiary_; }

 private:
  void calculateHash_();
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

struct PbftBlockCert {
  PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  explicit PbftBlockCert(dev::RLP const& all_rlp);
  explicit PbftBlockCert(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<Vote> cert_votes;
  bytes rlp() const;
  static void encode_raw(dev::RLPStream& rlp, PbftBlock const& pbft_blk, dev::bytesConstRef votes_raw);
};
std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b);

}  // namespace taraxa