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
  const auto last_blk_num =
      db->lookup_int<EthBlockNumber>(final_chain::DBMetaKeys::LAST_NUMBER, DB::Columns::final_chain_meta);
  if (!last_blk_num || *last_blk_num == 0) return;
  auto block_num = *last_blk_num;

  std::unique_lock lock(mutex_);

  while (price_list_.capacity() != price_list_.size() && block_num) {
    auto trxs = db->getPeriodTransactions(block_num);
    // TODO [1640]
    assert(trxs);
    block_num--;

    // TODO [1640]
    // Light node
    // if (!trxs) {
    //   break;
    // }

    // Empty block
    if (!trxs->size()) {
      continue;
    }

    if (const auto min_trx =
            *std::min_element(trxs->begin(), trxs->end(),
                              [](const auto& t1, const auto& t2) { return t1.getGasPrice() < t2.getGasPrice(); });
        min_trx.getGasPrice()) {
      price_list_.push_front(std::move(min_trx.getGasPrice()));
    }
  }

  if (price_list_.empty()) return;

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