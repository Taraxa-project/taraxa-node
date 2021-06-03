#pragma once

#include "common/types.hpp"
namespace taraxa {

class ProposalPeriodDagLevelsMap {
 public:
  ProposalPeriodDagLevelsMap();
  ProposalPeriodDagLevelsMap(uint64_t period, level_t level_start, level_t level_end);
  explicit ProposalPeriodDagLevelsMap(bytes const &rlp_bytes);
  explicit ProposalPeriodDagLevelsMap(dev::RLP const &rlp);

  bytes rlp() const;

  const level_t max_levels_per_period = 100;  // Default max DAG levels for all DAG blocks within one period
  uint64_t proposal_period;                   // Start at 0, proposal period is not PBFT period
  std::pair<level_t, level_t> levels_interval;
};

}  // namespace taraxa
