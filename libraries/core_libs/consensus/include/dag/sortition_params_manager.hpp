#pragma once

#include "pbft/sync_block.hpp"
#include "storage/storage.hpp"
#include "vdf/config.hpp"

namespace taraxa {

struct SortitionParamsChange {
  uint64_t period = 0;
  VrfParams vrf_params;
  uint16_t interval_efficiency = 0;
  uint16_t actual_correction_per_percent = 1;

  SortitionParamsChange() = default;
  SortitionParamsChange(uint64_t period, uint16_t efficiency, VrfParams const& vrf);
  SortitionParamsChange(uint64_t period, uint16_t efficiency, VrfParams const& vrf,
                        SortitionParamsChange const& previous);
  static SortitionParamsChange from_rlp(dev::RLP const& rlp);
  bytes rlp() const;

  /**
   * Calculates needed correction of VRF params per DAG efficiency percent
   * This calculates parameter change which is necessary to change the efficiency by 1 percent. For example:
   * Current efficiency is 72%. Based on past calculations, we changed the parameter to 100.
   * The interval efficiency after these changes was 74%. It turns out that it has changed by 2%, but not the 3% as we
   * need Therefore, the required changePerPercent is equal `100 / 2 = 50`, and not `100 / 3 = 33.(3)` as it was before
   */
  uint16_t correctionPerPercent(SortitionParamsChange const& previous) const;
};

class SortitionParamsManager {
  SortitionConfig config_;
  std::shared_ptr<DbStorage> db_;
  std::vector<uint16_t> dag_efficiencies_;
  std::deque<SortitionParamsChange> params_changes_;
  SortitionParamsChange calculateChange(const uint64_t period);
  int32_t getChange(const uint64_t period, const uint16_t efficiency) const;
  void cleanup();

 public:
  SortitionParamsManager(SortitionConfig sort_conf, std::shared_ptr<DbStorage> db);
  SortitionParams getSortitionParams(std::optional<uint64_t> for_period = {}) const;
  uint64_t currentProposalPeriod() const;
  /**
   * Calculates the average correction from previous changes we store in memory.
   * We can regulate corrections count by removing oldest elements of  params_changes_ collection
   */
  uint16_t averageCorrectionPerPercent() const;

  /**
   * Calculating DAG efficiency in passed SyncBlock(PBFT)
   * DAG efficiency is count of unique transactions over all DAG blocks in a PBFT block divided by the count over all
   * DAG blocks in a PBFT block.
   */
  uint16_t calculateDagEfficiency(const SyncBlock& block) const;

  /**
   * Called from PbftManager.pushPbftBlock_()
   * Calculates and stores needed data on every block.
   * Every `computation_interval` blocks from config launches `recalculate` based on previous data
   */
  void pbftBlockPushed(const SyncBlock& block, DbStorage::Batch& batch);

  /**
   * Calculate average DAG efficiency from dag_efficiencies_
   */
  uint16_t averageDagEfficiency();

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa