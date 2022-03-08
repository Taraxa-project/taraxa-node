#pragma once

#include <boost/circular_buffer.hpp>

#include "final_chain/final_chain.hpp"

namespace taraxa {

class GasPricer {
 public:
  GasPricer(uint64_t percentile = 60, uint64_t number_of_blocks = 100, std::shared_ptr<DbStorage> db = {});
  ~GasPricer();

  u256 bid() const;
  void update(const Transactions& trxs);

 private:
  void init(std::shared_ptr<DbStorage> db);

  const uint64_t percentile_;

  mutable std::shared_mutex mutex_;
  u256 latest_price_ = 1;
  boost::circular_buffer<u256> price_list_;

  std::unique_ptr<std::thread> init_daemon_;
};

}  // namespace taraxa