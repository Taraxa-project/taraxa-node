#include <gtest/gtest.h>

#include "final_chain/contract_interface.hpp"
#include "util_test/gtest.hpp"

namespace taraxa::core_tests {

struct ABITest : BaseTest {};

using namespace taraxa::final_chain;

// based on https://docs.soliditylang.org/en/v0.8.10/abi-spec.html#examples
TEST_F(ABITest, abi_encoding) {
  EXPECT_EQ(dev::fromHex("0xcdcd77c0"), ContractInterface::getFunction("baz(uint32,bool)"));
  dev::bytes data;
  ContractInterface::pack(data, 69);
  EXPECT_EQ(dev::fromHex("0x0000000000000000000000000000000000000000000000000000000000000045"), data);
  data.clear();
  ContractInterface::pack(data, true);
  EXPECT_EQ(dev::fromHex("0x0000000000000000000000000000000000000000000000000000000000000001"), data);
  data.clear();
  ContractInterface::pack(data, addr_t("0x9da168dd6f1e0d4fe15736a65af68d9b9c772a1b"));
  EXPECT_EQ(dev::fromHex("0x0000000000000000000000009da168dd6f1e0d4fe15736a65af68d9b9c772a1b"), data);
  data.clear();
  ContractInterface::pack(data, dev::asBytes("def"));
  EXPECT_EQ(dev::fromHex("0x6465660000000000000000000000000000000000000000000000000000000000"), data);

  EXPECT_EQ(
      dev::fromHex(
          "0xcdcd77c000000000000000000000000000000000000000000000000000000000000000450000000000000000000000000000000000"
          "0000000000000000000000000000010000000000000000000000009da168dd6f1e0d4fe15736a65af68d9b9c772a1b"),
      ContractInterface::pack("baz(uint32,bool)", 69, true, addr_t("0x9da168dd6f1e0d4fe15736a65af68d9b9c772a1b")));
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();

  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  addr_t node_addr;
  logging.InitLogging(node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}