#pragma once

#include <boost/circular_buffer.hpp>
#include <shared_mutex>

#include "config/genesis.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {

class DbStorage;
class TransactionManager;

/** @addtogroup Transaction
 * @{
 */

/**
 * @brief Basic gas price calculator. We calculate the gas price based on the lowest price in last number_of_blocks
 * blocks. Then all of those values are sorted and current price is selected based on percentile
 *
 */
class GasPricer {
 public:
  GasPricer(const GenesisConfig &config, bool is_light_node = false, bool is_blocks_gas_pricer = false,
            std::shared_ptr<TransactionManager> trx_mgr = nullptr, std::shared_ptr<DbStorage> db = {});
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
  void update(const SharedTransactions &trxs);

 private:
  /**
   * @brief initialize values on start from blocks in DB
   *
   * @param db
   */
  void init(const std::shared_ptr<DbStorage> &db);

  const uint64_t kPercentile;
  const u256 kMinimumPrice;
  const bool kIsLightNode;

  mutable std::shared_mutex mutex_;
  u256 latest_price_;
  boost::circular_buffer<u256> price_list_;

  std::unique_ptr<std::thread> init_daemon_;
  const bool kBlocksGasPricer;
  std::shared_ptr<TransactionManager> trx_mgr_;
};

/** @}*/

}  // namespace taraxa