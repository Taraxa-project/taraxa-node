#pragma once

#include <string>

#include "vrf_sortition.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

/**
 * @brief Vote class is a vote class that includes vote hash, vote on PBFT block hash, vote signature, VRF sortition,
 * voter public key, voter address, and vote weight.
 */
class Vote {
 public:
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  Vote() = default;
  Vote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& blockhash);

  explicit Vote(dev::RLP const& rlp);
  explicit Vote(bytes const& rlp);
  bool operator==(Vote const& other) const { return rlp() == other.rlp(); }

  /**
   * @brief Get vote hash
   * @return vote hash
   */
  const vote_hash_t& getHash() const { return vote_hash_; }

  /**
   * @brief Get voter public key
   * @return voter public key
   */
  const public_t& getVoter() const {
    if (!cached_voter_) cached_voter_ = dev::recover(vote_signature_, sha3(false));
    return cached_voter_;
  }

  /**
   * @brief Get voter address
   * @return voter address
   */
  const addr_t& getVoterAddr() const {
    if (!cached_voter_addr_) cached_voter_addr_ = dev::toAddress(getVoter());
    return cached_voter_addr_;
  }

  /**
   * @brief Verify VRF sortition
   * @return true if passed
   */
  bool verifyVrfSortition(const vrf_pk_t& pk) const { return vrf_sortition_.verify(pk); }

  /**
   * @brief Get VRF sortition
   * @return VRF sortition
   */
  const VrfPbftSortition& getVrfSortition() const { return vrf_sortition_; }

  /**
   * @brief Get VRF sortition proof
   * @return VRF sortition proof
   */
  const vrf_wrapper::vrf_proof_t& getSortitionProof() const { return vrf_sortition_.proof_; }

  /**
   * @brief Get VRF sortition output
   * @return VRF sortition output
   */
  const vrf_wrapper::vrf_output_t& getCredential() const { return vrf_sortition_.output_; }

  /**
   * @brief Get vote signature
   * @return vote signature
   */
  const sig_t& getVoteSignature() const { return vote_signature_; }

  /**
   * @brief Get PBFT block hash that votes on
   * @return PBFT block hash
   */
  const blk_hash_t& getBlockHash() const { return blockhash_; }

  /**
   * @brief Get vote type
   * @return vote type
   */
  PbftVoteTypes getType() const { return vrf_sortition_.pbft_msg_.getType(); }

  /**
   * @brief Get vote PBFT period
   * @return vote PBFT period
   */
  PbftPeriod getPeriod() const { return vrf_sortition_.pbft_msg_.period_; }

  /**
   * @brief Get vote PBFT round
   * @return vote PBFT round
   */
  PbftRound getRound() const { return vrf_sortition_.pbft_msg_.round_; }

  /**
   * @brief Get vote PBFT step
   * @return vote PBFT step
   */
  PbftStep getStep() const { return vrf_sortition_.pbft_msg_.step_; }

  /**
   * @brief Recursive Length Prefix
   * @param inc_sig if include vote signature
   * @param inc_weight if include vote weight
   * @return bytes of RLP stream
   */
  bytes rlp(bool inc_sig = true, bool inc_weight = false) const;

  /**
   * @brief Verify vote
   * @return true if passed
   */
  bool verifyVote() const {
    auto pk = getVoter();
    return !pk.isZero();  // recoverd public key means that it was verified
  }

  /**
   * @brief Calculate vote weight
   * @param stake voter DPOS eligible votes count
   * @param dpos_total_votes_count total DPOS votes count
   * @param threshold PBFT sortition threshold that is minimum of between PBFT committee size and total DPOS votes count
   * @return vote weight
   */
  uint64_t calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold) const {
    assert(stake);
    weight_ = vrf_sortition_.calculateWeight(stake, dpos_total_votes_count, threshold, getVoter());
    return weight_.value();
  }

  /**
   * @brief Get vote weight
   * @return vote weight
   */
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
  /**
   * @brief Secure Hash Algorithm 3
   * @param inc_sig if include vote signature
   * @return secure hash as vote hash
   */
  vote_hash_t sha3(bool inc_sig) const { return dev::sha3(rlp(inc_sig)); }

  vote_hash_t vote_hash_;  // hash of this vote
  blk_hash_t blockhash_;   // Voted PBFT block hash
  sig_t vote_signature_;
  VrfPbftSortition vrf_sortition_;
  mutable public_t cached_voter_;
  mutable addr_t cached_voter_addr_;
  mutable std::optional<uint64_t> weight_;
};

/**
 * @brief VotesBundle struct stores a bunch of votes that vote on the same voting value in the specific PBFT round and
 * step, the total votes weights must be greater or equal to PBFT 2t+1.
 */
struct VotesBundle {
  blk_hash_t voted_block_hash;
  PbftPeriod votes_period{0};
  std::vector<std::shared_ptr<Vote>> votes;  // Greater than 2t+1 votes

  VotesBundle() {}
  VotesBundle(blk_hash_t const& voted_block_hash, std::vector<std::shared_ptr<Vote>> const& votes)
      : voted_block_hash(voted_block_hash), votes(votes) {}
};

/** @}*/

}  // namespace taraxa