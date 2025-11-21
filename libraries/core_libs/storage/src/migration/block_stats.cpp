#include "storage/migration/block_stats.hpp"

#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <chrono>

#include "common/thread_pool.hpp"
#include "storage/storage.hpp"
namespace taraxa::storage::migration {

BlockStats::BlockStats(std::shared_ptr<DbStorage> db, const FullNodeConfig& config)
    : migration::Base(db), kConfig(config) {}

std::string BlockStats::id() { return "BlockStats"; }

uint32_t BlockStats::dbVersion() { return 1; }

class OldBlockStats : public rewards::BlockStats {
 public:
  RLP_FIELDS_DEFINE_INPLACE(block_author_, validators_stats_, total_dag_blocks_count_, total_votes_weight_,
                            max_votes_weight_)
};

// Migrate block_rewards_stats columns from old format (OldBlockStats) to the new format (rewards::BlockStats).
// rewards::BlockStats contains new variable "blocks_per_year_", which does not need to be set to sme specific value
// here
void BlockStats::migrate(logger::Logger& log) {
  auto it = std::unique_ptr<rocksdb::Iterator>(db_->getColumnIterator(DbStorage::Columns::block_rewards_stats));
  if (it->SeekToFirst(); it->Valid()) {
    const auto first_block_stats_rlp = dev::RLP(it->value().ToString());
    if (first_block_stats_rlp.itemCount() != 5) {
      LOG(log) << "Block reward data already saved in new format";
      return;
    }
  } else {
    LOG(log) << "No block reward data found";
    return;
  }

  std::map<PbftPeriod, rewards::BlockStats> migrated_blocks_stats;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    PbftPeriod period;
    memcpy(&period, it->key().data(), sizeof(PbftPeriod));

    // This should never happen - if it does, it means user deployed version with cacti hardfork functionality after
    // cacti hardfork block num and he needs to resync from the snapshot, which was taken before cacti hf. It is not
    // possible to fix such node without resync
    if (kConfig.genesis.state.hardforks.isOnCactiHardfork(period)) {
      LOG(log) << "Trying to apply block reward data migration introduced in cacti hardfork for the period >= cacti "
                  "hardfork block num. "
                  "Need to resync from the snapshot created before cacti hardfork";
      assert(false);
      throw std::runtime_error("Node in broken state. Need to resync from snapshot taken before cacti hardfork");
    }

    // OldBlockStats is derived from rewards::BlockStats with the only difference - different rlp parsing methods
    // Note: Polymorphism via pointers is not used on purpose - OldBlockStats is just "temporary" class for parsing, no
    //       class members or functions should be preserved. OldBlockStats is sliced down to rewards::BlockStats
    migrated_blocks_stats[period] = util::rlp_dec<OldBlockStats>(dev::RLP(it->value().ToString()));
  }

  // Remove old block stats from db
  db_->deleteColumnData(DbStorage::Columns::block_rewards_stats);

  // Save new blocks stats into db
  for (const auto& block_stats : migrated_blocks_stats) {
    db_->saveBlockRewardsStats(block_stats.first, block_stats.second, batch_);
  }
  db_->commitWriteBatch(batch_);
  db_->compactColumn(DbStorage::Columns::block_rewards_stats);
}

}  // namespace taraxa::storage::migration