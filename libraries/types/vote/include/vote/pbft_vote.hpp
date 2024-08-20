#pragma once

#include <json/value.h>

#include "common/vrf_wrapper.hpp"
#include "vote.hpp"
#include "vrf_sortition.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

/**
 * @brief PbftVote class is a vote class that includes vote hash, vote on PBFT block hash, vote signature, VRF
 * sortition, voter public key, voter address, and vote weight.
 */
class PbftVote : public Vote {
 public:
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  PbftVote() = default;
  PbftVote(secret_t const& node_sk, VrfPbftSortition vrf_sortition, blk_hash_t const& block_hash);

  // Ctor for optimized rlp vote objects - only signature and vrf proof are in the rlp
  explicit PbftVote(const blk_hash_t& block_hash, PbftPeriod period, PbftRound round, PbftStep step,
                    dev::RLP const& rlp);

  // Ctors for full rlp vote objects - all data are encoded in the rlp
  explicit PbftVote(dev::RLP const& rlp);
  explicit PbftVote(bytes const& rlp);

  bool operator==(const PbftVote& other) const;

  /**
   * @brief Verify VRF sortition
   * @return true if passed
   */
  bool verifyVrfSortition(const vrf_pk_t& pk, bool strict) const;

  /**
   * @brief Get VRF sortition
   * @return VRF sortition
   */
  const VrfPbftSortition& getVrfSortition() const;

  /**
   * @brief Get VRF sortition proof
   * @return VRF sortition proof
   */
  const vrf_wrapper::vrf_proof_t& getSortitionProof() const;

  /**
   * @brief Get VRF sortition output
   * @return VRF sortition output
   */
  const vrf_wrapper::vrf_output_t& getCredential() const;

  /**
   * @brief Get vote type
   * @return vote type
   */
  PbftVoteTypes getType() const;

  /**
   * @brief Get vote PBFT period
   * @return vote PBFT period
   */
  PbftPeriod getPeriod() const;

  /**
   * @brief Get vote PBFT round
   * @return vote PBFT round
   */
  PbftRound getRound() const;

  /**
   * @brief Get vote PBFT step
   * @return vote PBFT step
   */
  PbftStep getStep() const;

  /**
   * @brief Recursive Length Prefix
   * @param inc_sig if include vote signature
   * @param inc_weight if include vote weight
   * @return bytes of RLP stream
   */
  bytes rlp(bool inc_sig = true, bool inc_weight = false) const;

  /**
   * @brief Optimized Recursive Length Prefix
   * @note Encode only vote's signature and vrf proof into the rlp
   *
   * @return bytes of RLP stream
   */
  bytes optimizedRlp() const;

  /**
   * @brief Calculate vote weight
   * @param stake voter DPOS eligible votes count
   * @param dpos_total_votes_count total DPOS votes count
   * @param threshold PBFT sortition threshold that is minimum of between PBFT committee size and total DPOS votes count
   * @return vote weight
   */
  uint64_t calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold) const;

  /**
   * @brief Get vote weight
   * @return vote weight
   */
  std::optional<uint64_t> getWeight() const;

  friend std::ostream& operator<<(std::ostream& strm, PbftVote const& vote) {
    strm << "[Vote] " << std::endl;
    strm << "  vote_hash: " << vote.vote_hash_ << std::endl;
    strm << "  voter: " << vote.getVoter() << std::endl;
    strm << "  vote_signature: " << vote.vote_signature_ << std::endl;
    strm << "  blockhash: " << vote.block_hash_ << std::endl;
    if (vote.weight_) strm << "  weight: " << vote.weight_.value() << std::endl;
    strm << "  vrf_sortition: " << vote.vrf_sortition_ << std::endl;
    return strm;
  }

  /**
   * @brief Get vote in JSON representation
   * @return vote JSON
   */
  Json::Value toJSON() const;

 private:
  /**
   * @brief Secure Hash Algorithm 3
   * @param inc_sig if include vote signature
   * @return secure hash as vote hash
   */
  vote_hash_t sha3(bool inc_sig) const override;

  VrfPbftSortition vrf_sortition_;
  mutable std::optional<uint64_t> weight_;
};

/** @}*/

}  // namespace taraxa