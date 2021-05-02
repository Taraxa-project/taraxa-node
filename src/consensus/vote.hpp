#pragma once

#include <libdevcrypto/Common.h>

#include <deque>
#include <optional>
#include <string>

#include "common/types.hpp"
#include "config/config.hpp"
#include "consensus/pbft_chain.hpp"
#include "util/util.hpp"
#include "vrf_wrapper.hpp"

namespace taraxa {
class FullNode;
class PbftManager;

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

  blk_hash_t blk;  // Last PBFT block hash in chain
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
  sig_t getVoteSignature() const { return vote_signature_; }
  blk_hash_t getBlockHash() const { return blockhash_; }
  PbftVoteTypes getType() const { return vrf_sortition_.pbft_msg.type; }
  uint64_t getRound() const { return vrf_sortition_.pbft_msg.round; }
  size_t getStep() const { return vrf_sortition_.pbft_msg.step; }
  bytes rlp(bool inc_sig = true) const;
  bool verifyVote() const {
    auto msg = sha3(false);
    voter();
    return dev::verify(cached_voter_, vote_signature_, msg);
  }
  bool verifyCanSpeak(size_t threshold, size_t valid_players) const {
    return vrf_sortition_.canSpeak(threshold, valid_players);
  }

  friend std::ostream& operator<<(std::ostream& strm, Vote const& vote) {
    strm << "[Vote] " << std::endl;
    strm << "  vote_hash: " << vote.vote_hash_ << std::endl;
    strm << "  voter: " << vote.getVoter() << std::endl;
    strm << "  vote_signatue: " << vote.vote_signature_ << std::endl;
    strm << "  blockhash: " << vote.blockhash_ << std::endl;
    strm << "  vrf_sorition: " << vote.vrf_sortition_ << std::endl;
    return strm;
  }

 private:
  blk_hash_t sha3(bool inc_sig) const { return dev::sha3(rlp(inc_sig)); }
  void voter() const;

  vote_hash_t vote_hash_;  // hash of this vote
  blk_hash_t blockhash_;   // Voted PBFT block hash
  sig_t vote_signature_;
  VrfPbftSortition vrf_sortition_;
  mutable public_t cached_voter_;
};

class VoteManager {
 public:
  VoteManager(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<PbftChain> pbft_chain);
  ~VoteManager() {}

  // Unverified votes
  void addUnverifiedVote(Vote const& vote);
  void addUnverifiedVotes(std::vector<Vote> const& votes);
  void removeUnverifiedVote(uint64_t const& pbft_round, vote_hash_t const& vote_hash);
  bool voteInUnverifiedMap(uint64_t const& pbft_round, vote_hash_t const& vote_hash);
  std::vector<Vote> getUnverifiedVotes();
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;

  // Verified votes
  void addVerifiedVote(Vote const& vote);
  bool voteInVerifiedMap(uint64_t const& pbft_round, vote_hash_t const& vote_hash);
  void clearVerifiedVotesTable();
  std::vector<Vote> getVerifiedVotes();

  std::vector<Vote> getVerifiedVotes(uint64_t const pbft_round, blk_hash_t const& last_pbft_block_hash,
                                     size_t const sortition_threshold, uint64_t eligible_voter_count,
                                     std::function<bool(addr_t const&)> const& is_eligible);

  void cleanupVotes(uint64_t pbft_round);

  bool voteValidation(blk_hash_t const& last_pbft_block_hash, Vote const& vote, size_t const valid_sortition_players,
                      size_t const sortition_threshold) const;

  bool pbftBlockHasEnoughValidCertVotes(PbftBlockCert const& pbft_block_and_votes, size_t valid_sortition_players,
                                        size_t sortition_threshold, size_t pbft_2t_plus_1) const;

  std::string getJsonStr(std::vector<Vote> const& votes);

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // <pbft_round, <vote_hash, vote>>
  std::map<uint64_t, std::unordered_map<vote_hash_t, Vote>> unverified_votes_;
  std::map<uint64_t, std::unordered_map<vote_hash_t, Vote>> verified_votes_;

  mutable boost::shared_mutex unverified_votes_access_;
  mutable boost::shared_mutex verified_votes_access_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;

  LOG_OBJECTS_DEFINE;
};

class NextVotesForPreviousRound {
 public:
  NextVotesForPreviousRound(addr_t node_addr, std::shared_ptr<DbStorage> db);

  void clear();

  bool find(vote_hash_t next_vote_hash);

  bool enoughNextVotes() const;

  bool haveEnoughVotesForNullBlockHash() const;

  blk_hash_t getVotedValue() const;

  std::vector<Vote> getNextVotes();

  size_t getNextVotesSize() const;

  void addNextVotes(std::vector<Vote> const& next_votes, size_t const pbft_2t_plus_1);

  void update(std::vector<Vote> const& next_votes, size_t const pbft_2t_plus_1);

  void updateWithSyncedVotes(std::vector<Vote> const& votes, size_t const pbft_2t_plus_1);

 private:
  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  void assertError_(std::vector<Vote> next_votes_1, std::vector<Vote> next_votes_2) const;

  mutable boost::shared_mutex access_;

  std::shared_ptr<DbStorage> db_;

  bool enough_votes_for_null_block_hash_;
  blk_hash_t voted_value_;  // For value is not null block hash
  size_t next_votes_size_;
  // <voted PBFT block hash, next votes list that have greater or equal to 2t+1 votes voted at the PBFT block hash>
  // only save votes >= 2t+1 voted at same value in map and set
  std::unordered_map<blk_hash_t, std::vector<Vote>> next_votes_;
  std::unordered_set<vote_hash_t> next_votes_set_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
