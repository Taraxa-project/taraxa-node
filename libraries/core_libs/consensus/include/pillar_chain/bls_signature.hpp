#pragma once

#include <bls/bls.h>
#include <libdevcore/RLP.h>

#include <optional>

#include "common/types.hpp"
#include "pillar_chain/pillar_block.hpp"

namespace taraxa::pillar_chain {

/** @addtogroup PILLAR_CHAIN
 * @{
 */

/**
 * @brief BlsSignature
 */
class BlsSignature {
 public:
  using Hash = uint256_hash_t;

  constexpr static size_t kRlpSize{4};

 public:
  BlsSignature() = default;
  BlsSignature(const dev::RLP& rlp);
  BlsSignature(PillarBlock::Hash pillar_block_hash, PbftPeriod period, const addr_t& validator,
               const libff::alt_bn128_Fr& secret);

  /**
   * @brief Validates BLS signature
   *
   * @param bls_puk_key
   * @return
   */
  bool isValid(const std::shared_ptr<libff::alt_bn128_G2>& bls_pub_key) const;

  /**
   * @return bls signature rlp
   * @param include_sig
   */
  dev::bytes getRlp(bool include_sig = true) const;

  /**
   * @return Optimized rlp containing only signer address + signature
   *
   * @note It is used for saving 2t+1 bls signatures where pillar block hash & period is known
   */
  // dev::bytes getOptimizedRlp() const;

  /**
   * @return bls signature hash
   */
  Hash getHash() const;

  /**
   * @return pillar block hash
   */
  PillarBlock::Hash getPillarBlockHash() const;

  /**
   * @return signature pbft period
   */
  PbftPeriod getPeriod() const;

  /**
   * @return bls signature author address
   */
  addr_t getSignerAddr() const;

  /**
   * @return actual bls signature
   */
  libff::alt_bn128_G1 getSignature() const;

  HAS_RLP_FIELDS
 private:
  PillarBlock::Hash pillar_block_hash_{0};
  // Pbft period of pillar block & period during which was the signature created
  PbftPeriod period_;
  addr_t signer_addr_;
  libff::alt_bn128_G1 signature_;

  mutable Hash kCachedHash;
};

/** @}*/

}  // namespace taraxa::pillar_chain
