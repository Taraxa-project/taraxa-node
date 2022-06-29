#include <gtest/gtest.h>

#include "final_chain/contract_interface.hpp"
#include "util_test/gtest.hpp"

namespace taraxa::core_tests {

struct ABITest : BaseTest {};

using namespace taraxa::final_chain;

// based on https://docs.soliditylang.org/en/v0.8.10/abi-spec.html#examples
TEST_F(ABITest, abi_encoding) {
  EXPECT_EQ(dev::fromHex("0xcdcd77c0"), ContractInterface::getFunction("baz(uint32,bool)"));
  EXPECT_EQ(ContractInterface::pack(69),
            dev::fromHex("0x0000000000000000000000000000000000000000000000000000000000000045"));
  EXPECT_EQ(ContractInterface::pack(true),
            dev::fromHex("0x0000000000000000000000000000000000000000000000000000000000000001"));
  EXPECT_EQ(ContractInterface::pack(addr_t("0x9da168dd6f1e0d4fe15736a65af68d9b9c772a1b")),
            dev::fromHex("0x0000000000000000000000009da168dd6f1e0d4fe15736a65af68d9b9c772a1b"));
  EXPECT_EQ(
      dev::fromHex(
          "0xcdcd77c000000000000000000000000000000000000000000000000000000000000000450000000000000000000000000000000000"
          "0000000000000000000000000000010000000000000000000000009da168dd6f1e0d4fe15736a65af68d9b9c772a1b"),
      ContractInterface::packFunctionCall("baz(uint32,bool)", 69, true,
                                          addr_t("0x9da168dd6f1e0d4fe15736a65af68d9b9c772a1b")));
}

TEST_F(ABITest, abi_dynamic_encoding) {
  EXPECT_EQ(ContractInterface::pack(dev::asBytes("Hello, world!")),
            dev::fromHex("0x000000000000000000000000000000000000000000000000000000000000000d48656c6c6f2c20776f726c64210"
                         "0000000000000000000000000000000000000"));
  EXPECT_EQ(ContractInterface::pack(dev::asBytes("")),
            dev::fromHex("0x0000000000000000000000000000000000000000000000000000000000000000"));

  EXPECT_EQ(ContractInterface::pack(dev::fromHex("0xf9aad20feab5c2c3f0d9655fe22e65288d04b8faa925db55dc2d6b0390e8d1192ff"
                                                 "5b95dcc5dad1ea0e0e3e96af4c569a76aad5b083dc91e53f4874ee5170d861c")),
            dev::fromHex("0000000000000000000000000000000000000000000000000000000000000041f9aad20feab5c2c3f0d9655fe22e6"
                         "5288d04b8faa925db55dc2d6b0390e8d1192ff5b95dcc5dad1ea0e0e3e96af4c569a76aad5b083dc91e53f4874ee5"
                         "170d861c00000000000000000000000000000000000000000000000000000000000000"));

  EXPECT_EQ(dev::fromHex("0x939fdda8"),
            ContractInterface::getFunction("registerValidator(address,bytes,uint16,string,string)"));
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