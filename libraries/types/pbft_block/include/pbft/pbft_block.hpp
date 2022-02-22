#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"
#include "vote/vote.hpp"

namespace taraxa {

class PbftBlock {
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  blk_hash_t order_hash_;
  uint64_t period_;  // Block index, PBFT head block is period 0, first PBFT block is period 1
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;

 public:
  PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot, blk_hash_t const& order_hash,
            uint64_t period, addr_t const& beneficiary, secret_t const& sk);
  explicit PbftBlock(dev::RLP const& rlp);
  explicit PbftBlock(bytes const& RLP);
  explicit PbftBlock(std::string const& JSON);

  blk_hash_t sha3(bool include_sig) const;
  std::string getJsonStr() const;
  Json::Value getJson() const;
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;
  bytes rlp(bool include_sig) const;

  static Json::Value toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks);

  auto const& getBlockHash() const { return block_hash_; }
  auto const& getPrevBlockHash() const { return prev_block_hash_; }
  auto const& getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }
  auto const& getOrderHash() const { return order_hash_; }
  auto getPeriod() const { return period_; }
  auto getTimestamp() const { return timestamp_; }
  auto const& getBeneficiary() const { return beneficiary_; }

 private:
  void calculateHash_();
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

}  // namespace taraxa