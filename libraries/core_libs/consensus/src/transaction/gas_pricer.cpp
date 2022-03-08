#include "transaction/gas_pricer.hpp"

namespace taraxa {

GasPricer::GasPricer(uint64_t percentile, uint64_t number_of_blocks, std::shared_ptr<DbStorage> db)
    : percentile_(percentile), price_list_(number_of_blocks) {
  if (db) {
    init_daemon_ = std::make_unique<std::thread>([this, db_ = std::move(db)]() { init(std::move(db_)); });
  }
}

GasPricer::~GasPricer() {
  if (init_daemon_) init_daemon_->join();
}

u256 GasPricer::bid() const {
  std::shared_lock lock(mutex_);
  return latest_price_;
}

void GasPricer::init(std::shared_ptr<DbStorage> db) {
  std::unique_lock lock(mutex_);
  const auto number_of_blocks = db->getNumBlockExecuted();
  size_t start = 0;
  size_t end = number_of_blocks;
  auto missing_periods = false;
  if (number_of_blocks == 0) {
    return;
  } else if (number_of_blocks > price_list_.capacity()) {
    start = number_of_blocks - price_list_.capacity();
    end = start + price_list_.capacity();
  }
  for (size_t i = start; i < end; i++) {
    auto trxs = db->getPeriodTransactions(i);
    assert(trxs);

    if (!trxs->size()) {
      missing_periods = true;
      continue;
    }

    if (const auto min_trx =
            *std::min_element(trxs->begin(), trxs->end(),
                              [](const auto& t1, const auto& t2) { return t1.getGasPrice() < t2.getGasPrice(); });
        min_trx.getGasPrice()) {
      price_list_.push_back(std::move(min_trx.getGasPrice()));
    }
  }
  if (missing_periods) {
    // TODO
  }

  std::vector<u256> sorted_prices;
  sorted_prices.reserve(price_list_.size());
  std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
  std::sort(sorted_prices.begin(), sorted_prices.end());

  if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * percentile_ / 100]) {
    latest_price_ = std::move(new_price);
  }
}

void GasPricer::update(const Transactions& trxs) {
  if (trxs.empty()) return;

  std::vector<u256> prices;
  prices.reserve(trxs.size());

  std::transform(trxs.begin(), trxs.end(), std::back_inserter(prices),
                 [](const auto& trx) { return trx.getGasPrice(); });

  if (u256 minimum = *std::min_element(prices.begin(), prices.end())) {
    std::unique_lock lock(mutex_);

    price_list_.push_back(std::move(minimum));

    std::vector<u256> sorted_prices;
    sorted_prices.reserve(price_list_.size());
    std::copy(price_list_.begin(), price_list_.end(), std::back_inserter(sorted_prices));
    std::sort(sorted_prices.begin(), sorted_prices.end());

    if (auto new_price = sorted_prices[(sorted_prices.size() - 1) * percentile_ / 100]) {
      latest_price_ = std::move(new_price);
    }
  }
}

}  // namespace taraxa