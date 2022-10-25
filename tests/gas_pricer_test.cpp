#include "transaction/gas_pricer.hpp"

#include <gtest/gtest.h>

namespace taraxa::core_tests {

// Do not use BaseTest from "util_test/gtest.hpp" as its functionality is not needed in this test
struct BaseTest : virtual testing::Test {
  testing::UnitTest* current_test = ::testing::UnitTest::GetInstance();
  testing::TestInfo const* current_test_info = current_test->current_test_info();

  BaseTest() = default;
  virtual ~BaseTest() = default;

  BaseTest(const BaseTest&) = delete;
  BaseTest(BaseTest&&) = delete;
  BaseTest& operator=(const BaseTest&) = delete;
  BaseTest& operator=(BaseTest&&) = delete;
};

struct GasPricerTest : BaseTest {};

const auto secret = secret_t::random();

TEST_F(GasPricerTest, basic_test) {
  GasPricer gp(GasPriceConfig{});
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

TEST_F(GasPricerTest, random_test) {
  std::srand(std::time(nullptr));

  const size_t percentile = 10 + std::rand() % 50;
  const size_t number_of_blocks = 100 + std::rand() % 200;
  std::vector<size_t> prices;
  prices.reserve(number_of_blocks);

  GasPricer gp({percentile, number_of_blocks, 0});

  for (size_t i = 0; i < number_of_blocks; ++i) {
    const size_t gas_price = 1 + std::rand() % 1000;
    gp.update({std::make_shared<Transaction>(0, 0, gas_price, 0, bytes(), secret)});
    prices.push_back(gas_price);
  }

  std::sort(prices.begin(), prices.end());
  EXPECT_EQ(gp.bid(), prices[(prices.size() - 1) * percentile / 100]);
}

}  // namespace taraxa::core_tests