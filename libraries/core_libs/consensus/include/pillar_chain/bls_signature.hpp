#pragma once

#include "common/types.hpp"
#include <bls/bls.h>
#include <libdevcore/RLP.h>
#include <optional>
#include "pillar_chain/pillar_block.hpp"

namespace taraxa {


/** @addtogroup PILLAR_CHAIN
 * @{
 */

/**
 * @brief BlsSignature
 */
class BlsSignature {
 public:
  using Signature = libff::alt_bn128_G1;
  using Hash = uint256_hash_t;

 public:
  BlsSignature(const dev::RLP& rlp);
  BlsSignature(PillarBlock::Hash pillar_block_hash, const libff::alt_bn128_G2& public_key, const libff::alt_bn128_Fr& secret);

  /**
   * @brief Validates BLS signature
   *
   * @return true if valid, otherwise false
   */
  bool isValid() const;

  /**
   * @return bls signature rlp
   */
  dev::bytes getRlp() const;

  /**
   * @return bls signature hash
   */
  Hash getHash() const;

  PillarBlock::Hash getPillarBlockHash() const;

 private:
  PillarBlock::Hash pillar_block_hash_{0};

  Signature signature_;
  // TODO: seems like pub key is needed for verification of sig ???
  libff::alt_bn128_G2 public_key_;

  mutable std::optional<Hash> cached_hash_;
};

/** @}*/

}  // namespace taraxa
