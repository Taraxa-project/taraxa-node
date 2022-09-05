#pragma once

#include "common/default_construct_copyable_movable.hpp"
#include "vdf/sortition.hpp"

namespace taraxa {

using std::string;
using VdfSortition = vdf_sortition::VdfSortition;

/**
 * @brief DagBlock class is a DAG block class which main data is a list of transaction hashes included in the block and
 * hashes of pivot and tips blocks it points to
 *
 */
class DagBlock {
  // !!! Important: Any change in order or types of DagBlock class members will result in broken RLP ctor and some
  // methods
  //                as it is parsed with strong assumption of which member is at which position in rlp bytes
  //                representation. See "DagBlock(dev::RLP const &_rlp)" or "extract_dag_level_from_rlp". If some change
  //                needs to be done in class members, please check carefully all possible dependencies it has like for
  //                example in mentioned methods...
  blk_hash_t pivot_;
  level_t level_ = 0;
  vec_blk_t tips_;
  vec_trx_t trxs_;  // transactions
  uint64_t gas_estimation_;
  sig_t sig_;
  u256 block_weight_;
  mutable blk_hash_t hash_;
  mutable util::DefaultConstructCopyableMovable<std::mutex> hash_mu_;
  uint64_t timestamp_ = 0;
  vdf_sortition::VdfSortition vdf_;
  mutable addr_t cached_sender_;  // block creater
  mutable util::DefaultConstructCopyableMovable<std::mutex> cached_sender_mu_;
  static const size_t kLevelPosInRlp{1};
  static const size_t kSigPosInRlp{6};

 public:
  DagBlock() = default;
  // fixme: This constructor is bogus, used only in tests. Eliminate it
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, sig_t signature,
           blk_hash_t hash, addr_t sender);
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash,
           addr_t sender);
  // fixme: used only in tests, Eliminate it
  DagBlock(blk_hash_t const &pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, secret_t const &sk);
  DagBlock(blk_hash_t const &pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, VdfSortition vdf,
           secret_t const &sk);
  explicit DagBlock(Json::Value const &doc);
  explicit DagBlock(string const &json);
  explicit DagBlock(dev::RLP const &_rlp);
  explicit DagBlock(dev::bytes const &_rlp) : DagBlock(dev::RLP(_rlp)) {}

  /**
   * @brief Extracts dag level from rlp representation
   *
   * @param rlp
   * @return
   */
  static level_t extract_dag_level_from_rlp(const dev::RLP &rlp);

  /**
   * @brief Extracts signature from rlp representation
   *
   * @param rlp
   * @return signature
   */
  static sig_t extract_signature_from_rlp(const dev::RLP &rlp);

  friend std::ostream &operator<<(std::ostream &str, DagBlock const &u) {
    str << "	pivot		= " << u.pivot_.abridged() << std::endl;
    str << "	level		= " << u.level_ << std::endl;
    str << "	tips ( " << u.tips_.size() << " )	= ";
    for (auto const &t : u.tips_) str << t.abridged() << " ";
    str << std::endl;
    str << "	trxs ( " << u.trxs_.size() << " )	= ";
    for (auto const &t : u.trxs_) str << t.abridged() << " ";
    str << std::endl;
    str << "	signature	= " << u.sig_.abridged() << std::endl;
    str << "	hash		= " << u.getHash().abridged() << std::endl;
    str << "	sender		= " << u.getSender().abridged() << std::endl;
    str << "  vdf = " << u.vdf_ << std::endl;
    return str;
  }
  bool operator==(DagBlock const &other) const { return this->sha3(true) == other.sha3(true); }

  auto const &getPivot() const { return pivot_; }
  auto getLevel() const { return level_; }
  auto getTimestamp() const { return timestamp_; }
  auto const &getTips() const { return tips_; }
  auto const &getTrxs() const { return trxs_; }
  auto const &getGasEstimation() const { return gas_estimation_; }
  auto const &getSig() const { return sig_; }
  blk_hash_t const &getHash() const;
  uint16_t getDifficulty() const { return vdf_.getDifficulty(); }

  addr_t const &getSender() const;
  Json::Value getJson(bool with_derived_fields = true) const;
  std::string getJsonStr() const;

  bool verifySig() const;
  void verifyVdf(const SortitionParams &vdf_config, const h256 &proposal_period_hash,
                 const vrf_wrapper::vrf_pk_t &pk) const;
  bytes rlp(bool include_sig) const;

  /**
   * @brief Returns dag block data rlp stream
   *
   * @param include_sig
   * @return dev::RLPStream
   */
  dev::RLPStream streamRLP(bool include_sig) const;

 private:
  blk_hash_t sha3(bool include_sig) const;
};

struct DagFrontier {
  DagFrontier() = default;
  DagFrontier(blk_hash_t const &pivot, vec_blk_t const &tips) : pivot(pivot), tips(tips) {}
  void clear() {
    pivot.clear();
    tips.clear();
  }

  bool isEqual(const DagFrontier &frontier) {
    if (pivot != frontier.pivot) {
      return false;
    }

    return std::equal(tips.begin(), tips.end(), frontier.tips.begin(), frontier.tips.end());
  }

  blk_hash_t pivot;
  vec_blk_t tips;
};

}  // namespace taraxa
