#include "transaction/gas_pricer.hpp"

namespace taraxa {

GasPricer::GasPricer(uint64_t percentile, uint64_t number_of_blocks, bool is_light_node, std::shared_ptr<DbStorage> db)
    : kPercentile_(percentile), kIsLightNode_(is_light_node), price_list_(number_of_blocks) {
  assert(kPercentile_ <= 100);
  if (db) {
    init_daemon_ = std::make_unique<std::thread>([this, db_ = std::move(db)]() { init(db_); });
  }
}

GasPricer::~GasPricer() {
  if (init_daemon_ && init_daemon_->joinable()) init_daemon_->join();
}

u256 GasPricer::bid() const {
  std::shared_lock lock(mutex_);
  return latest_price_;
}

void GasPricer::init(const std::shared_ptr<DbStorage>& db) {
  const auto last_blk_num =
      db->lookup_int<EthBlockNumber>(final_chain::DBMetaKeys::LAST_NUMBER, DB::Columns::final_chain_meta);
  if (!last_blk_num || *last_blk_num == 0) return;
  auto block_num = *last_blk_num;

  std::unique_lock lock(mutex_);

  while (price_list_.capacity() != price_list_.size() && block_num) {
    auto trxs = db->getPeriodTransactions(block_num);
    block_num--;

    assert(kIsLightNode_ || trxs);

    // Light node
    if (kIsLightNode_ && !trxs) {
      break;
    }

    // Empty block
    if (!trxs->size()) {
      continue;
    }

    if (const auto min_trx =
            *std::min_element(trxs->begin(), trxs->end(),
                              [](const auto& t1, const auto& t2) { return t1.getGasPrice() < t2.getGasPrice(); });
        min_trx.getGasPrice()) {
      price_list_.push_front(min_trx.getGasPrice());
    }
  }

  if (price_list_.empty()) return;

  std::vector<u256> sorted_prices;
  sorted_prices.reserve(price_list_.size());
  std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
  std::sort(sorted_prices.begin(), sorted_prices.end());

  if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * kPercentile_ / 100]) {
    latest_price_ = std::move(new_price);
  }
}

void GasPricer::update(const Transactions& trxs) {
  if (trxs.empty()) return;

  if (const auto min_trx = *std::min_element(
          trxs.begin(), trxs.end(), [](const auto& t1, const auto& t2) { return t1.getGasPrice() < t2.getGasPrice(); });
      min_trx.getGasPrice()) {
    std::unique_lock lock(mutex_);

    price_list_.push_back(min_trx.getGasPrice());

    std::vector<u256> sorted_prices;
    sorted_prices.reserve(price_list_.size());
    std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
    std::sort(sorted_prices.begin(), sorted_prices.end());

    if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * kPercentile_ / 100]) {
      latest_price_ = std::move(new_price);
    }
  }
}

}  // namespace taraxa