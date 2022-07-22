#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <boost/multiprecision/mpfr.hpp>

#include "common/types.hpp"
#include "common/vrf_wrapper.hpp"

namespace taraxa {

/** @addtogroup Vote
 * @{
 */

enum PbftVoteTypes : uint8_t { propose_vote_type = 0, soft_vote_type, cert_vote_type, next_vote_type };

/**
 * @brief VrfPbftMsg struct uses vote type, PBFT round, and PBFT step to generate a message for doing VRF sortition.
 */
class VrfPbftMsg {
 public:
  VrfPbftMsg() = default;
  VrfPbftMsg(PbftVoteTypes type, uint64_t period, uint64_t round, size_t step);

  /**
   * @brief Combine vote type, PBFT period, round and step to a string
   * @return a string of vote type, PBFT period, round and step
   */
  std::string toString() const;

  /**
   * @brief Get bytes of RLP stream
   * @return bytes of RLP stream
   */
  bytes getRlpBytes() const;

  bool operator==(VrfPbftMsg const& other) const;
  friend std::ostream& operator<<(std::ostream& strm, VrfPbftMsg const& pbft_msg) {
    strm << "  [Vrf Pbft Msg] " << std::endl;
    strm << "    type: " << static_cast<uint32_t>(pbft_msg.type) << std::endl;
    strm << "    period: " << pbft_msg.period << std::endl;
    strm << "    round: " << pbft_msg.round << std::endl;
    strm << "    step: " << pbft_msg.step << std::endl;
    return strm;
  }

  PbftVoteTypes type;
  uint64_t period;
  uint64_t round;
  size_t step;
};

/**
 * @brief Get a hash number of combining VRF output, voter address, and vote weight index
 * @param vrf VRF output
 * @param address voter address
 * @param index vote weight index
 * @return a hash number in Secure Hash Algorithm 3
 */
dev::h256 getVoterIndexHash(const vrf_wrapper::vrf_output_t& vrf, const public_t& address, uint64_t index = 0);

/**
 * @brief VrfPbftSortition class used for doing VRF sortition to place a vote or to propose a new PBFT block
 */
class VrfPbftSortition : public vrf_wrapper::VrfSortitionBase {
  using vrf_sk_t = vrf_wrapper::vrf_sk_t;
  using vrf_pk_t = vrf_wrapper::vrf_pk_t;
  using vrf_proof_t = vrf_wrapper::vrf_proof_t;
  using vrf_output_t = vrf_wrapper::vrf_output_t;

 public:
  VrfPbftSortition() = default;

  VrfPbftSortition(vrf_sk_t const& sk, const VrfPbftMsg& pbft_msg)
      : VrfSortitionBase(sk, pbft_msg.getRlpBytes()), pbft_msg_(pbft_msg) {}

  explicit VrfPbftSortition(dev::bytes const& rlp);

  /**
   * @brief Get bytes of RLP stream
   * @return bytes of RLP stream
   */
  dev::bytes getRlpBytes() const;

  /**
   * @brief Verify VRF sortition
   * @return true if passed
   */
  bool verify(const vrf_pk_t& pk) const { return VrfSortitionBase::verify(pk, pbft_msg_.getRlpBytes()); }

  bool operator==(VrfPbftSortition const& other) const {
    return pbft_msg_ == other.pbft_msg_ && vrf_wrapper::VrfSortitionBase::operator==(other);
  }

  static inline uint256_t max256bits = std::numeric_limits<uint256_t>::max();
  static inline auto kMax256bFP = max256bits.convert_to<boost::multiprecision::mpfr_float>();

  /**
   * @brief Calculate a vote weight in binominal distribution
   * @param stake voter DPOS eligible votes count
   * @param dpos_total_votes_count total DPOS votes count
   * @param threshold PBFT sortition threshold that is minimum of between PBFT committee size and total DPOS votes count
   * @param hash a hash number of combining VRF output, voter address, and vote weight index
   * @return vote weight
   */
  static uint64_t getBinominalDistribution(uint64_t stake, double dpos_total_votes_count, double threshold,
                                           const uint256_t& hash);

  /**
   * @brief Calculate vote weight
   * @param stake voter DPOS eligible votes count
   * @param dpos_total_votes_count total DPOS votes count
   * @param threshold PBFT sortition threshold that is minimum of between PBFT committee size and total DPOS votes count
   * @param address voter public key
   * @return vote weight
   */
  uint64_t calculateWeight(uint64_t stake, double dpos_total_votes_count, double threshold,
                           const public_t& address) const;

  friend std::ostream& operator<<(std::ostream& strm, const VrfPbftSortition& vrf_sortition) {
    strm << "[VRF sortition] " << std::endl;
    strm << "  proof: " << vrf_sortition.proof_ << std::endl;
    strm << "  output: " << vrf_sortition.output_ << std::endl;
    strm << vrf_sortition.pbft_msg_ << std::endl;
    return strm;
  }

  VrfPbftMsg pbft_msg_;
};

/** @}*/

}  // namespace taraxa