#pragma once

#include <boost/circular_buffer.hpp>

#include "final_chain/final_chain.hpp"

namespace taraxa {
/**
 * @brief Basic gas price calculator. We calculate the gas price based on the lowest price in last number_of_blocks
 * blocks. Then all of those values are sorted and currnet price is selected based on percentile
 *
 */
class GasPricer {
 public:
  GasPricer(uint64_t percentile = 60, uint64_t number_of_blocks = 200, bool is_light_node = false,
            std::shared_ptr<DbStorage> db = {});
  ~GasPricer();

  GasPricer(const GasPricer &) = delete;
  GasPricer(GasPricer &&) = delete;
  GasPricer &operator=(const GasPricer &) = delete;
  GasPricer &operator=(GasPricer &&) = delete;

  /**
   * @brief returns current gas price
   *
   * @return u256 gas price
   */
  u256 bid() const;

  /**
   * @brief updates gas price after each executed block
   *
   * @param trxs from latest block
   */
  void update(const Transactions &trxs);

 private:
  /**
   * @brief initialize values on start from blocks in DB
   *
   * @param db
   */
  void init(const std::shared_ptr<DbStorage> &db);

  const uint64_t kPercentile_;
  const bool kIsLightNode_;

  mutable std::shared_mutex mutex_;
  u256 latest_price_ = 1;
  boost::circular_buffer<u256> price_list_;

  std::unique_ptr<std::thread> init_daemon_;
};

}  // namespace taraxa