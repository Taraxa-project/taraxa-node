#ifndef TARAXA_NODE__PBFT_CONFIG_HPP_
#define TARAXA_NODE__PBFT_CONFIG_HPP_

#include "types.hpp"

namespace taraxa {

struct PbftConfig {
  uint32_t lambda_ms_min = 0;
  uint32_t committee_size = 0;
  uint32_t dag_blocks_size = 0;
  uint32_t ghost_path_move_back = 0;
  bool run_count_votes = false;
};

}  // namespace taraxa

#endif  // TARAXA_NODE__PBFT_CONFIG_HPP_
