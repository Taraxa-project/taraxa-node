#pragma once

#include <string>

#include "vrf_sortition.hpp"

namespace taraxa {

class Vote {
 public:
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  Vote() = default;
  Vote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& blockhash);

  explicit Vote(dev::RLP const& rlp);
  explicit Vote(bytes const& rlp);
  bool operator==(Vote const& other) const { return rlp() == other.rlp(); }

  void validate(uint64_t stake, double dpos_total_votes_count, double sortition_threshold) const;
  vote_hash_t getHash() const { return vote_hash_; }
  public_t getVoter() const {
    if (!cached_voter_) cached_voter_ = dev::recover(vote_signature_, sha3(false));
    return cached_voter_;
  }
  addr_t getVoterAddr() const {
    if (!cached_voter_addr_) cached_voter_addr_ = dev::toAddress(getVoter());
    return cached_voter_addr_;
  }

  bool verifyVrfSortition() const { return vrf_sortition_.verify(); }
  auto getVrfSortition() const { return vrf_sortition_; }
  auto getSortitionProof() const { return vrf_sortition_.proof_; }
  auto getCredential() const { return vrf_sortition_.output_; }
  sig_t getVoteSignature() const { return vote_signature_; }
  blk_hash_t getBlockHash() const { return blockhash_; }
  PbftVoteTypes getType() const { return vrf_sortition_.pbft_msg_.type; }
  uint64_t getRound() const { return vrf_sortition_.pbft_msg_.round; }
  size_t getStep() const { return vrf_sortition_.pbft_msg_.step; }
  bytes rlp(bool inc_sig = true, bool inc_weight = false) const;
  bool verifyVote() const {
    auto pk = getVoter();
    return !pk.isZero();  // recoverd public key means that it was verified
  }

  uint64_t calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold) const {
    assert(stake);
    weight_ = vrf_sortition_.calculateWeight(stake, dpos_total_votes_count, threshold, getVoter());
    return weight_.value();
  }

  std::optional<uint64_t> getWeight() const { return weight_; }

  friend std::ostream& operator<<(std::ostream& strm, Vote const& vote) {
    strm << "[Vote] " << std::endl;
    strm << "  vote_hash: " << vote.vote_hash_ << std::endl;
    strm << "  voter: " << vote.getVoter() << std::endl;
    strm << "  vote_signatue: " << vote.vote_signature_ << std::endl;
    strm << "  blockhash: " << vote.blockhash_ << std::endl;
    if (vote.weight_) strm << "  weight: " << vote.weight_.value() << std::endl;
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
  mutable std::optional<uint64_t> weight_;
};

struct VotesBundle {
  bool enough;
  blk_hash_t voted_block_hash;
  std::vector<std::shared_ptr<Vote>> votes;  // exactly 2t+1 votes

  VotesBundle() : enough(false), voted_block_hash(blk_hash_t(0)) {}
  VotesBundle(bool enough, blk_hash_t const& voted_block_hash, std::vector<std::shared_ptr<Vote>> const& votes)
      : enough(enough), voted_block_hash(voted_block_hash), votes(votes) {}
};

}  // namespace taraxa