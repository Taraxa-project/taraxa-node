#ifndef TARAXA_NODE_VOTE_H
#define TARAXA_NODE_VOTE_H

#include <libdevcrypto/Common.h>

#include <deque>
#include <optional>
#include <string>

#include "config.hpp"
#include "pbft_chain.hpp"
#include "types.hpp"
#include "util.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {
class FullNode;
class PbftManager;

struct VrfPbftMsg : public vrf_wrapper::VrfMsgFace {
  VrfPbftMsg() = default;
  VrfPbftMsg(blk_hash_t const& blk, PbftVoteTypes type, uint64_t round, size_t step) : blk(blk), type(type), round(round), step(step) {}
  std::string toString() const override {
    return blk.toString() + "_" + std::to_string(type) + "_" + std::to_string(round) + "_" + std::to_string(step);
  }
  bool operator==(VrfPbftMsg const& other) const { return blk == other.blk && type == other.type && round == other.round && step == other.step; }
  friend std::ostream& operator<<(std::ostream& strm, VrfPbftMsg const& pbft_msg) {
    strm << "  [Vrf Pbft Msg] " << std::endl;
    strm << "    blk_hash: " << pbft_msg.blk << std::endl;
    strm << "    type: " << pbft_msg.type << std::endl;
    strm << "    round: " << pbft_msg.round << std::endl;
    strm << "    step: " << pbft_msg.step << std::endl;
    return strm;
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
  VrfPbftSortition(vrf_sk_t const& sk, VrfPbftMsg const& pbft_msg) : pbft_msg(pbft_msg), VrfSortitionBase(sk, pbft_msg) {}
  explicit VrfPbftSortition(bytes const& rlp);
  bytes getRlpBytes() const;
  bool verify() { return VrfSortitionBase::verify(pbft_msg); }
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
  bool verifyCanSpeak(size_t threshold, size_t valid_players) const { return vrf_sortition_.canSpeak(threshold, valid_players); }

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

class VoteManager {
 public:
  VoteManager(addr_t node_addr, std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain)
      : final_chain_(final_chain), pbft_chain_(pbft_chain) {
    LOG_OBJECTS_CREATE("VOTE_MGR");
  }
  ~VoteManager() {}

  bool voteValidation(blk_hash_t const& last_pbft_block_hash, Vote const& vote, size_t const valid_sortition_players,
                      size_t const sortition_threshold) const;
  bool addVote(taraxa::Vote const& vote);
  void cleanupVotes(uint64_t pbft_round);
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;
  // for unit test only
  std::vector<Vote> getVotes(uint64_t pbft_round, size_t eligible_voter_count, blk_hash_t last_pbft_block_hash, size_t sortition_threshold);
  std::vector<Vote> getVotes(uint64_t const pbft_round, blk_hash_t const& last_pbft_block_hash, size_t const sortition_threshold,
                             uint64_t eligible_voter_count, std::function<bool(addr_t const&)> const& is_eligible);
  std::string getJsonStr(std::vector<Vote> const& votes);
  std::vector<Vote> getAllVotes();
  bool pbftBlockHasEnoughValidCertVotes(PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players, size_t sortition_threshold,
                                        size_t pbft_2t_plus_1) const;

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // <pbft_round, <vote_hash, vote>>
  std::map<uint64_t, std::map<vote_hash_t, Vote>> unverified_votes_;

  mutable boost::shared_mutex access_;

  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa

#endif  // VOTE_H
