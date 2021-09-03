#include "dag/proposal_period_levels_map.hpp"

namespace taraxa {

ProposalPeriodDagLevelsMap::ProposalPeriodDagLevelsMap()
    : proposal_period(0), levels_interval(std::make_pair(0, max_levels_per_period)) {}

ProposalPeriodDagLevelsMap::ProposalPeriodDagLevelsMap(uint64_t period, level_t level_start, level_t level_end)
    : proposal_period(period), levels_interval(std::make_pair(level_start, level_end)) {}

ProposalPeriodDagLevelsMap::ProposalPeriodDagLevelsMap(bytes const &rlp_bytes)
    : ProposalPeriodDagLevelsMap(dev::RLP(rlp_bytes)) {}

ProposalPeriodDagLevelsMap::ProposalPeriodDagLevelsMap(dev::RLP const &r) {
  dev::RLP const rlp(r);
  proposal_period = rlp[0].toInt<uint64_t>();
  levels_interval = std::make_pair(rlp[1].toInt<level_t>(), rlp[2].toInt<level_t>());
}

bytes ProposalPeriodDagLevelsMap::rlp() const {
  dev::RLPStream s(3);
  s << proposal_period;
  s << levels_interval.first;
  s << levels_interval.second;

  return s.out();
}

}  // namespace taraxa
