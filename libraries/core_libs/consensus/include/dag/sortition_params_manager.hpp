#pragma once

#include "pbft/sync_block.hpp"
#include "storage/storage.hpp"
#include "vdf/config.hpp"

namespace taraxa {

using EfficienciesMap = std::map<uint16_t, int32_t>;

/** @addtogroup DAG
 * @{
 */

/**
 * @brief Changing vrf params for DAG blocks generation
 */
struct SortitionParamsChange {
  uint64_t period = 0;
  VrfParams vrf_params;
  uint16_t interval_efficiency = 0;

  SortitionParamsChange() = default;
  SortitionParamsChange(uint64_t period, uint16_t efficiency, const VrfParams& vrf);
  static SortitionParamsChange from_rlp(const dev::RLP& rlp);
  bytes rlp() const;
};

class SortitionParamsManager {
 public:
  SortitionParamsManager(const addr_t& node_addr, SortitionConfig sort_conf, std::shared_ptr<DbStorage> db);
  SortitionParams getSortitionParams(std::optional<uint64_t> for_period = {}) const;

  /**
   * Calculating DAG efficiency in passed SyncBlock(PBFT)
   * DAG efficiency is count of unique transactions over all DAG blocks in a PBFT block divided by the count over all
   * DAG blocks in a PBFT block.
   * @param block SyncBlock to calculate its efficiency
   * @returns DAG efficiency
   */
  uint16_t calculateDagEfficiency(const SyncBlock& block) const;

  /**
   * Called from PbftManager.pushPbftBlock_()
   * Calculates and stores needed data on every block.
   * Every `computation_interval` blocks from config launches `recalculate` based on previous data
   * @param block SyncBlock to process
   * @param batch DB batch in which all changes  will be added
   * @param non_empty_pbft_chain_size PBFT chain size excluding pbft blocks with null anchor
   */
  void pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch, size_t non_empty_pbft_chain_size);

  /**
   * Calculate average DAG efficiency from dag_efficiencies_. Used at the end of interval.
   * @returns average DAG efficiency in current interval
   */
  uint16_t averageDagEfficiency();

  /**
   * Get memory params changes, used only for unit tests
   * @returns params changes
   */
  const std::deque<SortitionParamsChange>& getParamsChanges() const { return params_changes_; }

 protected:
  SortitionConfig config_;
  std::shared_ptr<DbStorage> db_;
  std::deque<uint16_t> dag_efficiencies_;
  uint32_t ignored_efficiency_counter_ = 0;
  std::deque<SortitionParamsChange> params_changes_;
  SortitionParamsChange calculateChange(uint64_t period);
  EfficienciesMap getEfficienciesToUpperRange(uint16_t efficiency, int32_t threshold) const;
  int32_t getNewUpperRange(uint16_t efficiency) const;
  void cleanup();

  LOG_OBJECTS_DEFINE
};
/** @}*/

}  // namespace taraxa