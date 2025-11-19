#include "plugin/light.hpp"

#include "common/config_exception.hpp"
#include "config/config.hpp"
#include "dag/dag_manager.hpp"

namespace taraxa::plugin {

namespace bpo = boost::program_options;
constexpr auto HISTORY = "light.history";
constexpr auto NO_STATE_DB_PRUNING = "light.no_state_db_pruning";
constexpr auto NO_LIVE_CLEANUP = "light.no_live_cleanup";

Light::Light(std::shared_ptr<AppBase> app_) : Plugin(app_), history_(app()->getMutableConfig().light_node_history) {}

void Light::init(const boost::program_options::variables_map &opts) {
  auto node_addr = app()->getAddress();
  LOG_OBJECTS_CREATE("light");
  const auto &conf = app()->getConfig();

  const auto min_light_node_history_ =
      (conf.genesis.state.dpos.blocks_per_year * conf.kDefaultLightNodeHistoryDays) / 365;
  if (!opts[HISTORY].empty()) {
    history_ = opts[HISTORY].as<uint64_t>();
    if (history_ < min_light_node_history_) {
      throw ConfigException("Min. required light node history is " + std::to_string(min_light_node_history_) +
                            " blocks (" + std::to_string(conf.kDefaultLightNodeHistoryDays) + " days)");
    }
  } else {
    history_ = min_light_node_history_;
  }
  state_db_pruning_ = !opts[NO_STATE_DB_PRUNING].as<bool>();

  live_cleanup_ = !opts[NO_LIVE_CLEANUP].as<bool>();

  app()->getMutableConfig().is_light_node = true;
}

void Light::addOptions(boost::program_options::options_description &opts) {
  opts.add_options()(HISTORY, bpo::value<uint32_t>(), "Number of blocks to keep in light node history");
  opts.add_options()(NO_STATE_DB_PRUNING, bpo::bool_switch()->default_value(false), "Prune state_db");
  opts.add_options()(NO_LIVE_CLEANUP, bpo::bool_switch()->default_value(false), "Disable live cleanup");
}

void Light::start() {
  clearLightNodeHistory();
  if (state_db_pruning_) {
    pruneStateDb();
  }
  app()->getFinalChain()->block_finalized_.subscribe(
      [this](std::shared_ptr<final_chain::FinalizationResult>) {
        if (live_cleanup_) {
          if (live_cleanup_in_progress_) {
            return;
          }
          live_cleanup_in_progress_ = true;
          clearLightNodeHistory(true);
          live_cleanup_in_progress_ = false;
        }
      },
      cleanup_pool_);
}

void Light::shutdown() { cleanup_pool_->stop(); }

uint64_t Light::getCleanupPeriod(uint64_t dag_period, std::optional<uint64_t> proposal_period) const {
  return std::min(dag_period - history_, *proposal_period);
}

void Light::clearLightNodeHistory(bool live_cleanup) {
  LOG(log_nf_) << "Clear light node history: live_cleanup=" << live_cleanup << ", history_=" << history_;
  const auto dag_manager = app()->getDagManager();
  const auto db = app()->getDB();

  const auto dag_period = dag_manager->getLatestPeriod();
  const auto dag_expiry_level = dag_manager->getDagExpiryLevel();
  const auto max_levels_per_period = dag_manager->getMaxLevelsPerPeriod();
  bool dag_expiry_level_condition = dag_expiry_level > max_levels_per_period + 1;
  if (dag_period > history_ && dag_expiry_level_condition) {
    if (!live_cleanup) {
      LOG(log_nf_) << "Clear light node history: dag_period=" << dag_period << ", dag_expiry_level=" << dag_expiry_level
                   << ", max_levels_per_period=" << max_levels_per_period
                   << ", dag_expiry_level_condition=" << dag_expiry_level_condition << ", history_=" << history_;
    }
    const auto proposal_period = db->getProposalPeriodForDagLevel(dag_expiry_level - max_levels_per_period - 1);
    assert(proposal_period);

    // This prevents deleting any data needed for dag blocks proposal period, we only delete periods for the expired
    // dag blocks
    const uint64_t end = getCleanupPeriod(dag_period, proposal_period);
    uint64_t dag_level_to_keep = 1;
    if (dag_expiry_level > max_levels_per_period) {
      dag_level_to_keep = dag_expiry_level - max_levels_per_period;
    }

    clearHistory(end, dag_level_to_keep, live_cleanup);
    if (!live_cleanup) {
      LOG(log_nf_) << "Clear light node history completed";
    }
  }
}

void Light::clearNonBlockData(PbftPeriod start, PbftPeriod end, bool live_cleanup) {
  auto db = app()->getDB();
  auto length = end - start;
  if (!live_cleanup && length > 2 * kPeriodsToKeepNonBlockData) {
    recreateNonBlockData(end);
    return;
  }

  auto batch = db->createWriteBatch();
  for (PbftPeriod period = start; period < end; period++) {
    auto period_data = db->getPeriodData(period);
    if (!period_data.has_value()) {
      break;
    }
    for (auto t : period_data->transactions) {
      db->remove(batch, DbStorage::Columns::trx_period, t->getHash());
      db->remove(batch, DbStorage::Columns::final_chain_receipt_by_trx_hash, t->getHash());
    }
    for (auto d : period_data->dag_blocks) {
      db->remove(batch, DbStorage::Columns::dag_block_period, d->getHash());
    }
    db->remove(batch, DbStorage::Columns::pbft_block_period, period_data->pbft_blk->getBlockHash());
  }
  db->commitWriteBatch(batch);
}

void Light::recreateNonBlockData(PbftPeriod last_block_number) {
  auto db = app()->getDB();
  std::unordered_set<trx_hash_t> trxs;
  std::unordered_set<blk_hash_t> dag_blocks;
  std::unordered_set<blk_hash_t> pbft_blocks;

  for (uint64_t period = last_block_number - kPeriodsToKeepNonBlockData;; period++) {
    auto period_data = db->getPeriodData(period);
    if (!period_data.has_value()) {
      break;
    }
    for (auto t : period_data->transactions) {
      trxs.insert(t->getHash());
    }
    for (auto d : period_data->dag_blocks) {
      dag_blocks.insert(d->getHash());
    }
    pbft_blocks.insert(period_data->pbft_blk->getBlockHash());
  }

  db->clearColumnHistory(trxs, DbStorage::Columns::trx_period);
  db->clearColumnHistory(trxs, DbStorage::Columns::final_chain_receipt_by_trx_hash);
  db->clearColumnHistory(dag_blocks, DbStorage::Columns::dag_block_period);
  db->clearColumnHistory(pbft_blocks, DbStorage::Columns::pbft_block_period);
}

void Light::clearHistory(PbftPeriod end_period, uint64_t dag_level_to_keep, bool live_cleanup) {
  auto db = app()->getDB();
  auto it = db->getColumnIterator(DbStorage::Columns::period_data);
  // Find the first non-deleted period
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_period;
  memcpy(&start_period, it->key().data(), sizeof(uint64_t));
  if (start_period >= end_period) {
    return;
  }
  clearNonBlockData(start_period, end_period, live_cleanup);

  db->DeleteRange(DbStorage::Columns::period_data, start_period, end_period);
  db->DeleteRange(DbStorage::Columns::pillar_block, start_period, end_period);
  db->DeleteRange(DbStorage::Columns::final_chain_receipt_by_period, start_period, end_period);
  db->DeleteRange(DbStorage::Columns::period_lambda, start_period, end_period);
  db->CompactRange(DbStorage::Columns::period_data, start_period, end_period);
  db->CompactRange(DbStorage::Columns::pillar_block, start_period, end_period);
  db->CompactRange(DbStorage::Columns::final_chain_receipt_by_period, start_period, end_period);
  db->CompactRange(DbStorage::Columns::period_data, start_period, end_period);

  it = db->getColumnIterator(DbStorage::Columns::dag_blocks_level);
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }
  uint64_t start_level;
  memcpy(&start_level, it->key().data(), sizeof(uint64_t));

  uint64_t dag_level_end = dag_level_to_keep - 1;
  // Validate range before operations
  if (start_level >= dag_level_end) {
    return;
  }

  db->DeleteRange(DbStorage::Columns::dag_blocks_level, start_level, dag_level_end);
  db->CompactRange(DbStorage::Columns::dag_blocks_level, start_level, dag_level_end);
}

void Light::pruneStateDb() {
  const auto kPruneBlocksToKeep = kDagExpiryLevelLimit + kMaxLevelsPerPeriod + 1;
  // prune state db only if we have more than 2*kPruneBlocksToKeep blocks
  const uint64_t kPruneStateDbThreshold = 1.5 * kPruneBlocksToKeep;
  auto last_blk_num = app()->getFinalChain()->lastBlockNumber();
  if (last_blk_num > kPruneStateDbThreshold) {
    auto prune_block_num = last_blk_num - kPruneStateDbThreshold;
    auto prune_block = app()->getFinalChain()->blockHeader(prune_block_num);
    if (!prune_block) {
      LOG(log_nf_) << "Prune was done recently, skip state db pruning";
      return;
    }
    LOG(log_nf_) << "Pruning state db " << prune_block_num << ", this might take several minutes";
    app()->getFinalChain()->prune(prune_block_num);
    LOG(log_nf_) << "Pruning state db complete";
  }
}

}  // namespace taraxa::plugin
