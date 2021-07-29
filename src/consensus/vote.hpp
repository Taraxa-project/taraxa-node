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
class Network;
class SyncBlock;

struct VrfPbftMsg {
  VrfPbftMsg() = default;
  VrfPbftMsg(PbftVoteTypes type, uint64_t round, size_t step, size_t weighted_index)
      : type(type), round(round), step(step), weighted_index(weighted_index) {}

  std::string toString() const {
    return std::to_string(type) + "_" + std::to_string(round) + "_" + std::to_string(step) + "_" +
           std::to_string(weighted_index);
  }

  bool operator==(VrfPbftMsg const& other) const {
    return type == other.type && round == other.round && step == other.step && weighted_index == other.weighted_index;
  }

  friend std::ostream& operator<<(std::ostream& strm, VrfPbftMsg const& pbft_msg) {
    strm << "  [Vrf Pbft Msg] " << std::endl;
    strm << "    type: " << pbft_msg.type << std::endl;
    strm << "    round: " << pbft_msg.round << std::endl;
    strm << "    step: " << pbft_msg.step << std::endl;
    strm << "    weighted_index: " << pbft_msg.weighted_index << std::endl;
    return strm;
  }

  bytes getRlpBytes() const {
    dev::RLPStream s;
    s.appendList(4);
    s << static_cast<uint8_t>(type);
    s << round;
    s << step;
    s << weighted_index;
    return s.out();
  }

  PbftVoteTypes type;
  uint64_t round;
  size_t step;
  size_t weighted_index;
};

struct VrfPbftSortition : public vrf_wrapper::VrfSortitionBase {
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using vrf_output_t = vrf_wrapper::vrf_output_t;
  using bytes = dev::bytes;
  VrfPbftSortition() = default;
  VrfPbftSortition(vrf_sk_t const& sk, VrfPbftMsg const& pbft_msg)
      : VrfSortitionBase(sk, pbft_msg.getRlpBytes()), pbft_msg(pbft_msg) {}
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

  vote_hash_t getHash() const { return vote_hash_; }
  public_t getVoter() const {
    if (!cached_voter_) cached_voter_ = dev::recover(vote_signature_, sha3(false));
    return cached_voter_;
  }
  addr_t getVoterAddr() const {
    if (!cached_voter_addr_) cached_voter_addr_ = dev::toAddress(getVoter());
    return cached_voter_addr_;
  }

  bool verifyVrfSortition() { return vrf_sortition_.verify(); }
  auto getVrfSortition() const { return vrf_sortition_; }
  auto getSortitionProof() const { return vrf_sortition_.proof; }
  auto getCredential() const { return vrf_sortition_.output; }
  sig_t getVoteSignature() const { return vote_signature_; }
  blk_hash_t getBlockHash() const { return blockhash_; }
  PbftVoteTypes getType() const { return vrf_sortition_.pbft_msg.type; }
  uint64_t getRound() const { return vrf_sortition_.pbft_msg.round; }
  size_t getStep() const { return vrf_sortition_.pbft_msg.step; }
  size_t getWeightedIndex() const { return vrf_sortition_.pbft_msg.weighted_index; }
  bytes rlp(bool inc_sig = true) const;
  bool verifyVote() const {
    auto pk = getVoter();
    return !pk.isZero();  // recoverd public key means that it was verified
  }
  bool verifyCanSpeak(size_t threshold, size_t dpos_total_votes_count) const {
    return vrf_sortition_.canSpeak(threshold, dpos_total_votes_count);
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

  vote_hash_t vote_hash_;  // hash of this vote
  blk_hash_t blockhash_;   // Voted PBFT block hash
  sig_t vote_signature_;
  VrfPbftSortition vrf_sortition_;
  mutable public_t cached_voter_;
  mutable addr_t cached_voter_addr_;
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

  void updateNextVotes(std::vector<Vote> const& next_votes, size_t const pbft_2t_plus_1);

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

  LOG_OBJECTS_DEFINE
};

struct votesBundle {
  bool enough;
  blk_hash_t voted_block_hash;
  std::vector<Vote> votes;  // exactly 2t+1 votes

  votesBundle() : enough(false), voted_block_hash(blk_hash_t(0)) {}
  votesBundle(bool const enough_, blk_hash_t const& voted_block_hash_, std::vector<Vote> const& votes_)
      : enough(enough_), voted_block_hash(voted_block_hash_), votes(votes_) {}
};

class VoteManager {
 public:
  VoteManager(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<FinalChain> final_chain,
              std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr);
  ~VoteManager() {}

  void setNetwork(std::weak_ptr<Network> network);

  // Unverified votes
  bool addUnverifiedVote(Vote const& vote);
  void addUnverifiedVotes(std::vector<Vote> const& votes);
  void removeUnverifiedVote(uint64_t const pbft_round, vote_hash_t const& vote_hash);
  bool voteInUnverifiedMap(uint64_t const pbft_round, vote_hash_t const& vote_hash);
  std::vector<Vote> getUnverifiedVotes();
  void clearUnverifiedVotesTable();
  uint64_t getUnverifiedVotesSize() const;

  // Verified votes
  void addVerifiedVote(Vote const& vote);
  bool voteInVerifiedMap(Vote const& vote);
  void clearVerifiedVotesTable();
  uint64_t getVerifiedVotesSize() const;

  void removeVerifiedVotes();

  void verifyVotes(uint64_t const pbft_round, size_t const sortition_threshold, uint64_t const dpos_total_votes_count,
                   std::function<size_t(addr_t const&)> const& dpos_eligible_vote_count);

  void cleanupVotes(uint64_t const pbft_round);

  bool voteValidation(Vote& vote, size_t const valid_sortition_players, size_t const sortition_threshold) const;

  bool pbftBlockHasEnoughValidCertVotes(SyncBlock& pbft_block_and_votes, size_t valid_sortition_players,
                                        size_t sortition_threshold, size_t pbft_2t_plus_1) const;

  std::string getJsonStr(std::vector<Vote> const& votes);

  std::vector<Vote> getProposalVotes(uint64_t const pbft_round);

  votesBundle getVotesBundleByRoundAndStep(uint64_t const round, size_t const step, size_t const two_t_plus_one);

  uint64_t roundDeterminedFromVotes(size_t const two_t_plus_one);

 private:
  void retreieveVotes_();

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // <pbft_round, <vote_hash, vote>>
  std::map<uint64_t, std::unordered_map<vote_hash_t, Vote>> unverified_votes_;

  // <PBFT round, <PBFT step, <voted value, <vote hash list>>>>
  std::map<uint64_t, std::map<size_t, std::unordered_map<blk_hash_t, std::unordered_set<vote_hash_t>>>> verified_votes_;

  std::unordered_set<vote_hash_t> votes_invalid_in_current_final_chain_period_;
  h256 current_period_final_chain_block_hash_;
  std::map<addr_t, uint64_t> max_received_round_for_address_;

  std::unique_ptr<std::thread> daemon_;

  mutable boost::shared_mutex unverified_votes_access_;
  mutable boost::shared_mutex verified_votes_access_;

  addr_t node_addr_;

  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<FinalChain> final_chain_;
  std::shared_ptr<NextVotesForPreviousRound> previous_round_next_votes_;
  std::weak_ptr<Network> network_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
