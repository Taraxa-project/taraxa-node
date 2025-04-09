#include "transaction/gas_pricer.hpp"

#include "storage/storage.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

GasPricer::GasPricer(const GenesisConfig& config, bool is_light_node, bool is_blocks_gas_pricer,
                     std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DbStorage> db)
    : kPercentile(config.gas_price.percentile),
      kMinimumPrice(config.state.hardforks.soleirolia_hf.trx_min_gas_price),
      kIsLightNode(is_light_node),
      latest_price_(kMinimumPrice),
      price_list_(config.gas_price.blocks),
      kBlocksGasPricer(is_blocks_gas_pricer),
      trx_mgr_(std::move(trx_mgr)) {
  assert(kPercentile <= 100);
  if (db) {
    init_daemon_ = std::make_unique<std::thread>([this, db_ = std::move(db)]() { init(db_); });
  }
}

GasPricer::~GasPricer() {
  if (init_daemon_ && init_daemon_->joinable()) init_daemon_->join();
}

u256 GasPricer::bid() const {
  if (kBlocksGasPricer) {
    std::shared_lock lock(mutex_);
    return std::max(latest_price_, kMinimumPrice);
  } else {
    return std::max(trx_mgr_->getMinGasPriceForBlockInclusion(), kMinimumPrice);
  }
}

void GasPricer::init(const std::shared_ptr<DbStorage>& db) {
  if (!kBlocksGasPricer) return;
  const auto last_blk_num =
      db->lookup_int<EthBlockNumber>(DBMetaKeys::LAST_NUMBER, DbStorage::Columns::final_chain_meta);
  if (!last_blk_num || *last_blk_num == 0) return;
  auto block_num = *last_blk_num;

  std::unique_lock lock(mutex_);

  while (price_list_.capacity() != price_list_.size() && block_num) {
    auto trxs = db->getPeriodTransactions(block_num);
    block_num--;

    assert(kIsLightNode || trxs);

    // Light node
    if (kIsLightNode && !trxs) {
      break;
    }

    // Empty block
    if (!trxs->size()) {
      continue;
    }

    if (const auto min_trx =
            *std::min_element(trxs->begin(), trxs->end(),
                              [](const auto& t1, const auto& t2) { return t1->getGasPrice() < t2->getGasPrice(); });
        min_trx->getGasPrice()) {
      price_list_.push_front(min_trx->getGasPrice());
    }
  }

  if (price_list_.empty()) return;

  std::vector<u256> sorted_prices;
  sorted_prices.reserve(price_list_.size());
  std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
  std::sort(sorted_prices.begin(), sorted_prices.end());

  if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * kPercentile / 100]) {
    latest_price_ = std::move(new_price);
  }
}

void GasPricer::update(const SharedTransactions& trxs) {
  if (!kBlocksGasPricer || trxs.empty()) return;

  if (const auto min_trx =
          *std::min_element(trxs.begin(), trxs.end(),
                            [](const auto& t1, const auto& t2) { return t1->getGasPrice() < t2->getGasPrice(); });
      min_trx->getGasPrice()) {
    std::unique_lock lock(mutex_);
    price_list_.push_back(min_trx->getGasPrice());

    std::vector<u256> sorted_prices;
    sorted_prices.reserve(price_list_.size());
    std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
    std::sort(sorted_prices.begin(), sorted_prices.end());

    if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * kPercentile / 100]) {
      latest_price_ = std::move(new_price);
    }
  }
}

}  // namespace taraxa