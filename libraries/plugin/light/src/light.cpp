#include "plugin/light.hpp"

#include "common/config_exception.hpp"
#include "config/config.hpp"
#include "dag/dag_manager.hpp"

namespace taraxa::plugin {

namespace bpo = boost::program_options;
constexpr auto HISTORY = "light.history";
constexpr auto NO_STATE_DB_PRUNING = "light.no_state_db_pruning";
constexpr auto NO_LIVE_CLEANUP = "light.no_live_cleanup";

Light::Light(std::shared_ptr<AppBase> app)
    : Plugin(app),
      history_(app->getMutableConfig().light_node_history),
      logger_(logger::Logging::get().CreateChannelLogger("LIGHT")) {}

void Light::init(const boost::program_options::variables_map &opts) {
  const auto &conf = app()->getConfig();
  if (!opts[HISTORY].empty()) {
    history_ = opts[HISTORY].as<uint64_t>();
    if (history_ < min_light_node_history_) {
      throw ConfigException("Min. required light node history is " + std::to_string(min_light_node_history_) +
                            " blocks (" + std::to_string(conf.kDefaultLightNodeHistoryDays) + " days)");
    }
  } else {
    min_light_node_history_ = (conf.genesis.state.dpos.blocks_per_year * conf.kDefaultLightNodeHistoryDays) / 365;
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
          clearLightNodeHistory();
        }
      },
      cleanup_pool_);
}

void Light::shutdown() {}

uint64_t Light::getCleanupPeriod(uint64_t dag_period, std::optional<uint64_t> proposal_period) const {
  return std::min(dag_period - history_, *proposal_period);
}

void Light::clearLightNodeHistory() {
  const auto dag_manager = app()->getDagManager();
  const auto db = app()->getDB();

  const auto dag_period = dag_manager->getLatestPeriod();
  const auto dag_expiry_level = dag_manager->getDagExpiryLevel();
  const auto max_levels_per_period = dag_manager->getMaxLevelsPerPeriod();
  bool dag_expiry_level_condition = dag_expiry_level > max_levels_per_period + 1;
  if (dag_period > history_ && dag_expiry_level_condition) {
    const auto proposal_period = db->getProposalPeriodForDagLevel(dag_expiry_level - max_levels_per_period - 1);
    assert(proposal_period);

    // This prevents deleting any data needed for dag blocks proposal period, we only delete periods for the expired
    // dag blocks
    const uint64_t end = getCleanupPeriod(dag_period, proposal_period);
    uint64_t dag_level_to_keep = 1;
    if (dag_expiry_level > max_levels_per_period) {
      dag_level_to_keep = dag_expiry_level - max_levels_per_period;
    }

    clearHistory(end, dag_level_to_keep, dag_period);
  }
}

void Light::clearHistory(PbftPeriod end_period, uint64_t dag_level_to_keep, PbftPeriod last_block_number) {
  logger_->debug("Clear light node history: end_period={}, dag_level_to_keep={}, last_block_number={}", end_period,
                 dag_level_to_keep, last_block_number);
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

  db->DeleteRange(DbStorage::Columns::period_data, start_period, end_period);
  db->DeleteRange(DbStorage::Columns::pillar_block, start_period, end_period);
  db->DeleteRange(DbStorage::Columns::final_chain_receipt_by_period, start_period, end_period);
  db->CompactRange(DbStorage::Columns::period_data, start_period, end_period);
  db->CompactRange(DbStorage::Columns::pillar_block, start_period, end_period);
  db->CompactRange(DbStorage::Columns::final_chain_receipt_by_period, start_period, end_period);

  std::unordered_set<trx_hash_t> trxs;
  std::unordered_set<blk_hash_t> dag_blocks;
  std::unordered_set<blk_hash_t> pbft_blocks;

  for (uint64_t period = last_block_number - periods_to_keep_non_block_data_;; period++) {
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

  it = db->getColumnIterator(DbStorage::Columns::dag_blocks_level);
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_level;
  memcpy(&start_level, it->key().data(), sizeof(uint64_t));
  db->DeleteRange(DbStorage::Columns::dag_blocks_level, start_level, dag_level_to_keep - 1);
  db->CompactRange(DbStorage::Columns::dag_blocks_level, start_level, dag_level_to_keep - 1);
  logger_->info("Clear light node history completed");
}

void Light::pruneStateDb() {
  const auto kPruneBlocksToKeep = kDagExpiryLevelLimit + kMaxLevelsPerPeriod + 1;
  // prune state db only if we have more than 2*kPruneBlocksToKeep blocks
  const auto kPruneStateDbThreshold = 1.5 * kPruneBlocksToKeep;
  auto last_blk_num = app()->getFinalChain()->lastBlockNumber();
  if (last_blk_num > kPruneStateDbThreshold) {
    auto prune_block_num = last_blk_num - kPruneStateDbThreshold;
    auto prune_block = app()->getFinalChain()->blockHeader(prune_block_num);
    if (!prune_block) {
      logger_->info("Prune was done recently, skip state db pruning");
      return;
    }
    logger_->info("Pruning state db, this might take several minutes");
    app()->getFinalChain()->prune(prune_block_num);
    logger_->info("Pruning state db complete");
  }
}

}  // namespace taraxa::plugin
