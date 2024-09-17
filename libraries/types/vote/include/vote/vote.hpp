#pragma once

#include "common/types.hpp"

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
  Vote() = default;
  Vote(const blk_hash_t& block_hash);
  virtual ~Vote() = default;

  /**
   * @brief Sign the vote
   *
   * @param node_sk
   */
  void signVote(const secret_t& node_sk);

  /**
   * @brief Get vote hash
   * @return vote hash
   */
  const vote_hash_t& getHash() const;

  /**
   * @brief Get voter public key
   * @return voter public key
   */
  const public_t& getVoter() const;

  /**
   * @brief Get voter address
   * @return voter address
   */
  const addr_t& getVoterAddr() const;

  /**
   * @brief Get vote signature
   * @return vote signature
   */
  const sig_t& getVoteSignature() const;

  /**
   * @brief Get PBFT block hash that votes on
   * @return PBFT block hash
   */
  const blk_hash_t& getBlockHash() const;

  /**
   * @brief Verify vote
   * @return true if passed
   */
  bool verifyVote() const;

  /**
   * @brief Secure Hash Algorithm 3
   * @param inc_sig if include vote signature
   * @return secure hash as vote hash
   */
  virtual vote_hash_t sha3(bool inc_sig) const = 0;

 protected:
  blk_hash_t block_hash_;  // Voted block hash
  sig_t vote_signature_;

  mutable vote_hash_t vote_hash_;  // hash of this vote
  mutable public_t cached_voter_;
  mutable addr_t cached_voter_addr_;
};

/** @}*/

}  // namespace taraxa