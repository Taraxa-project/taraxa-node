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
  SyncBlock(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  SyncBlock(dev::RLP const& all_rlp);
  SyncBlock(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<Vote> cert_votes;
  std::vector<DagBlock> dag_blocks;
  std::vector<Transaction> transactions;
  bytes rlp() const;
};
std::ostream& operator<<(std::ostream& strm, SyncBlock const& b);

}  // namespace taraxa