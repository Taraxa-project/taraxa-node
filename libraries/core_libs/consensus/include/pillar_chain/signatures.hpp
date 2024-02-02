#pragma once

#include <memory>

#include "pillar_chain/bls_signature.hpp"

namespace taraxa::pillar_chain {

class Signatures {
 public:
  struct WeightSignatures {
    std::unordered_map<BlsSignature::Hash, std::shared_ptr<BlsSignature>> signatures;
    uint64_t weight{0};  // Signatures weight
  };

  struct PeriodSignatures {
    std::unordered_map<PillarBlock::Hash, WeightSignatures> pillar_block_signatures;
    std::unordered_map<addr_t, BlsSignature::Hash> unique_signers;

    // 2t+1 threshold for pillar block period
    uint64_t two_t_plus_one{0};
  };

 public:
  /**
   * @brief Checks if signature exists
   *
   * @param signature
   * @return true if exists, otherwise false
   */
  bool signatureExists(const std::shared_ptr<BlsSignature> signature) const;

  /**
   * @brief Checks if signature is unique per period & validator (signer)
   *
   * @param signature
   * @return true if unique, otherwise false
   */
  bool isUniqueBlsSig(const std::shared_ptr<BlsSignature> signature) const;

  /**
   * @brief Checks if there is 2t+1 signatures for specified period
   *
   * @param period
   * @return
   */
  bool hasTwoTPlusOneSignatures(PbftPeriod period, PillarBlock::Hash block_hash) const;

  /**
   * @brief Checks if specified period data have been already initialized
   *
   * @param period
   * @return true if initialized, othwrwise false
   */
  bool periodDataInitialized(PbftPeriod period) const;

  /**
   * @brief Initialize period data with period_two_t_plus_one
   *
   * @param period
   * @param period_two_t_plus_one
   */
  void initializePeriodData(PbftPeriod period, uint64_t period_two_t_plus_one);

  /**
   * @brief Add a signature to the bls signatures map
   * @param signature signature
   * @param signer_vote_count
   *
   * @return true if signature was successfully added, otherwise false
   */
  bool addVerifiedSig(const std::shared_ptr<BlsSignature>& signature, u_int64_t signer_vote_count);

  /**
   * @brief Get all bls signatures for specified pillar block
   *
   * @param period
   * @param pillar_block_hash
   * @param two_t_plus_one if true, return only if there is >= 2t+1 verified signatures
   *
   * @return all bls signatures for specified period and pillar block hash
   */
  std::vector<std::shared_ptr<BlsSignature>> getVerifiedBlsSignatures(PbftPeriod period,
                                                                      const PillarBlock::Hash pillar_block_hash,
                                                                      bool two_t_plus_one = false) const;

  /**
   * @brief Erases signatures wit period < min_period
   *
   * @param min_period
   */
  void eraseSignatures(PbftPeriod min_period);

 private:
  // Signatures for latest_pillar_block_.period - 1, latest_pillar_block_.period and potential +1 future pillar
  // block period
  std::map<PbftPeriod, PeriodSignatures> signatures_;
  mutable std::shared_mutex mutex_;
};

}  // namespace taraxa::pillar_chain