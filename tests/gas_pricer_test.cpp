#include "transaction/gas_pricer.hpp"

#include <gtest/gtest.h>

#include "config/config.hpp"
#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct GasPricerTest : NodesTest {};

const auto secret = secret_t::random();

TEST_F(GasPricerTest, basic_test) {
  GasPricer gp(GenesisConfig{}, false, true);
  EXPECT_EQ(gp.bid(), 1);

  gp.update({std::make_shared<Transaction>(0, 0, 1 /*gas_price*/, 0, bytes(), secret)});
  EXPECT_EQ(gp.bid(), 1);

  gp.update({std::make_shared<Transaction>(0, 0, 2 /*gas_price*/, 0, bytes(), secret)});
  EXPECT_EQ(gp.bid(), 1);

  gp.update({std::make_shared<Transaction>(0, 0, 3 /*gas_price*/, 0, bytes(), secret)});
  EXPECT_EQ(gp.bid(), 2);

  gp.update({std::make_shared<Transaction>(0, 0, 4 /*gas_price*/, 0, bytes(), secret)});
  EXPECT_EQ(gp.bid(), 2);

  gp.update({std::make_shared<Transaction>(0, 0, 5 /*gas_price*/, 0, bytes(), secret)});
  EXPECT_EQ(gp.bid(), 3);
}

TEST_F(GasPricerTest, basic_test_with_trx_pool) {
  TransactionQueue priority_queue(nullptr);
  auto trx = std::make_shared<Transaction>(1, 0, 2 /*gas_price*/, 100000, bytes(), secret);
  priority_queue.insert(std::move(trx), true, 1);

  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(90000), 3);
  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(110000), 1);

  auto trx2 = std::make_shared<Transaction>(2, 0, 3 /*gas_price*/, 200000, bytes(), secret);
  priority_queue.insert(std::move(trx2), true, 1);
  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(190000), 4);
  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(210000), 3);
  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(290000), 3);
  EXPECT_EQ(priority_queue.getMinGasPriceForBlockInclusion(310000), 1);
}

TEST_F(GasPricerTest, random_test) {
  std::srand(std::time(nullptr));

  GenesisConfig config;
  config.gas_price.percentile = 10 + std::rand() % 50;
  config.gas_price.blocks = 100 + std::rand() % 200;
  config.gas_price.minimum_price = 0;

  GasPricer gp(config, false, true);

  std::vector<size_t> prices;
  prices.reserve(config.gas_price.blocks);
  for (size_t i = 0; i < config.gas_price.blocks; ++i) {
    const size_t gas_price = 1 + std::rand() % 1000;
    gp.update({std::make_shared<Transaction>(0, 0, gas_price, 0, bytes(), secret)});
    prices.push_back(gas_price);
  }

  std::sort(prices.begin(), prices.end());
  EXPECT_EQ(gp.bid(), prices[(prices.size() - 1) * config.gas_price.percentile / 100]);
}

}  // namespace taraxa::core_tests