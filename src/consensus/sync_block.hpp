#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>

#include <map>
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
  SyncBlock(std::shared_ptr<PbftBlock> pbft_blk, std::vector<std::shared_ptr<Vote>> cert_votes);
  SyncBlock(dev::RLP&& all_rlp);
  SyncBlock(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<std::shared_ptr<Vote>> cert_votes;
  std::vector<DagBlock> dag_blocks;
  std::vector<Transaction> transactions;
  bytes rlp() const;
  void clear();
};
std::ostream& operator<<(std::ostream& strm, SyncBlock const& b);

}  // namespace taraxa