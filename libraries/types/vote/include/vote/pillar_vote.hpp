#pragma once

#include <libdevcore/RLP.h>

#include "common/encoding_rlp.hpp"
#include "vote.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

/**
 * @brief PillarVote class
 */
class PillarVote : public Vote {
 public:
  constexpr static size_t kNoSigRlpSize{2};
  constexpr static size_t kStandardRlpSize{3};

 public:
  PillarVote() = default;
  PillarVote(const secret_t& node_sk, PbftPeriod period, const blk_hash_t& block_hash);
  PillarVote(PbftPeriod period, const blk_hash_t& block_hash, sig_t&& signature);

  explicit PillarVote(const dev::RLP& rlp);
  explicit PillarVote(const bytes& rlp);

  /**
   * @return vote pbft period
   */
  PbftPeriod getPeriod() const;

  /**
   * @brief Recursive Length Prefix
   * @param inc_sig if include vote signature
   * @return bytes of RLP stream
   */
  bytes rlp(bool inc_sig = true) const;

  bytes encode(bool inc_sig = true) const;

  static PillarVote decode(const bytes& enc);

  HAS_RLP_FIELDS

 private:
  /**
   * @brief Secure Hash Algorithm 3
   * @param inc_sig if include vote signature
   * @return secure hash as vote hash
   */
  vote_hash_t sha3(bool inc_sig) const override;

 private:
  PbftPeriod period_;
};

/** @}*/

}  // namespace taraxa