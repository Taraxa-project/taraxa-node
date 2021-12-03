#pragma once

#include "common/default_construct_copyable_movable.hpp"
#include "vdf/sortition.hpp"

namespace taraxa {
using std::string;
using VdfSortition = vdf_sortition::VdfSortition;

// Note: Need to sign first then sender() and hash() is available
class DagBlock {
 public:
  DagBlock() = default;
  // fixme: This constructor is bogus, used only in tests. Eliminate it
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash,
           addr_t sender);
  DagBlock(blk_hash_t const &pivot, level_t &&level, vec_blk_t &&tips, vec_trx_t &&trxs,
           std::vector<vote_hash_t> &&votes_to_be_rewarded, VdfSortition &&vdf, secret_t const &sk);
  explicit DagBlock(Json::Value const &doc);
  explicit DagBlock(dev::RLP const &_rlp);
  explicit DagBlock(dev::bytes const &_rlp) : DagBlock(dev::RLP(_rlp)) {}

  /**
   * @brief Extracts dag level from rlp representation
   *
   * @param rlp
   * @return
   */
  static level_t extract_dag_level_from_rlp(const dev::RLP &rlp);

  friend std::ostream &operator<<(std::ostream &str, DagBlock const &u) {
    str << "	pivot		= " << u.pivot_.abridged() << std::endl;
    str << "	level		= " << u.level_ << std::endl;
    str << "	tips ( " << u.tips_.size() << " )	= ";
    for (auto const &t : u.tips_) str << t.abridged() << " ";
    str << std::endl;
    str << "	trxs ( " << u.trxs_.size() << " )	= ";
    for (auto const &t : u.trxs_) str << t.abridged() << " ";
    str << std::endl;
    str << "	votes_to_be_rewarded ( " << u.votes_to_be_rewarded_.size() << " )	= ";
    for (auto const &t : u.votes_to_be_rewarded_) str << t.abridged() << " ";
    str << std::endl;
    str << "	signature	= " << u.sig_.abridged() << std::endl;
    str << "	hash		= " << u.getHash().abridged() << std::endl;
    str << "	sender		= " << u.getSender().abridged() << std::endl;
    str << "  vdf = " << u.vdf_ << std::endl;
    return str;
  }
  bool operator==(DagBlock const &other) const { return this->sha3(true) == other.sha3(true); }

  const blk_hash_t &getPivot() const { return pivot_; }
  level_t getLevel() const { return level_; }
  uint64_t getTimestamp() const { return timestamp_; }
  const vec_blk_t &getTips() const { return tips_; }
  const vec_trx_t &getTrxs() const { return trxs_; }
  const std::vector<vote_hash_t> &getRewardsVotes() const { return votes_to_be_rewarded_; }
  const sig_t &getSig() const { return sig_; }
  const blk_hash_t &getHash() const;
  const vdf_sortition::VdfSortition &getVdf() const { return vdf_; }

  addr_t const &getSender() const;
  Json::Value getJson(bool with_derived_fields = true) const;
  std::string getJsonStr() const;

  bool verifySig() const;
  void verifyVdf(const SortitionParams &vdf_config) const;
  bytes rlp(bool include_sig) const;

 private:
  // How many rlp base fields has DagBlock rlp, signature is optional so it is not base field
  static constexpr size_t k_base_field_count = 7;

  // !!! Important: Any change in order or types of DagBlock class members will result in broken RLP ctor and some
  //                methods
  //                as it is parsed with strong assumption of which member is at which position in rlp bytes
  //                representation. See "DagBlock(dev::RLP const &_rlp)" or "extract_dag_level_from_rlp". If some
  //                change needs to be done in class members, please check carefully all possible dependencies it has:
  //                for example in mentioned methods...
  blk_hash_t pivot_;
  level_t level_ = 0;
  uint64_t timestamp_ = 0;
  vdf_sortition::VdfSortition vdf_;
  vec_blk_t tips_;
  vec_trx_t trxs_;  // transactions

  // List of votes hashes from previous (already finalized) pbft period that should be rewarded in the current period
  std::vector<vote_hash_t> votes_to_be_rewarded_;

  sig_t sig_;

  mutable blk_hash_t hash_;
  mutable util::DefaultConstructCopyableMovable<std::mutex> hash_mu_;

  mutable addr_t cached_sender_;  // block creater
  mutable util::DefaultConstructCopyableMovable<std::mutex> cached_sender_mu_;

  void streamRLP(dev::RLPStream &s, bool include_sig) const;
  blk_hash_t sha3(bool include_sig) const;
};

struct DagFrontier {
  DagFrontier() = default;
  DagFrontier(blk_hash_t const &pivot, vec_blk_t const &tips) : pivot(pivot), tips(tips) {}
  void clear() {
    pivot.clear();
    tips.clear();
  }
  blk_hash_t pivot;
  vec_blk_t tips;
};

}  // namespace taraxa
