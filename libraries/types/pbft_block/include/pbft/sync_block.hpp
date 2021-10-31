#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <vector>

#include "common/types.hpp"

namespace taraxa {

class Vote;
class PbftBlock;
struct Transaction;
class DagBlock;

class SyncBlock {
 public:
  SyncBlock() = default;
  SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> const& cert_votes);
  explicit SyncBlock(dev::RLP&& all_rlp);
  explicit SyncBlock(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<Vote>> cert_votes;
  std::vector<DagBlock> dag_blocks;
  std::vector<Transaction> transactions;
  bytes rlp() const;
  void clear();
  void hasEnoughValidCertVotes(size_t valid_sortition_players, size_t sortition_threshold, size_t pbft_2t_plus_1) const;
};
std::ostream& operator<<(std::ostream& strm, SyncBlock const& b);

}  // namespace taraxa