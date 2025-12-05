#include <gtest/gtest.h>
#include <libdevcore/RLP.h>

#include "test_util/test_util.hpp"

namespace taraxa::core_tests {

struct EncodingTest : NodesTest {};

TEST_F(EncodingTest, rlp_lifetime_safety) {
  auto getSerializedString = []() -> std::string {
    dev::RLPStream s;
    s.appendList(5);
    s << 10 << 20 << 30 << 40 << 50;
    auto bytes = s.out();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  };

  // Store the string, then create RLP
  {
    auto stored_string = getSerializedString();
    const auto rlp = dev::RLP(stored_string);
    auto stored_count = rlp.itemCount();
    EXPECT_EQ(stored_count, 5);
  }

  // Use bytes directly (already has rvalue protection)
  {
    dev::RLPStream s;
    s.appendList(5);
    s << 10 << 20 << 30 << 40 << 50;
    auto stored_bytes = s.out();  // Must store bytes
    const auto rlp = dev::RLP(stored_bytes);
    EXPECT_EQ(rlp.itemCount(), 5);
  }

  // The following patterns are now prevented at compile-time
  //
  // const auto rlp = dev::RLP(getSerializedString());
  // // Error: call to deleted constructor 'dev::RLP::RLP(std::string&&)'
  //
  // dev::RLPStream s;
  // s.appendList(5);
  // const auto rlp = dev::RLP(s.out());
  // // Error: call to deleted constructor 'dev::RLP::RLP(bytes const&&)'
}

TEST_F(EncodingTest, rlp_from_bytes) {
  // Test RLP construction from bytes vector
  dev::RLPStream stream;
  stream.appendList(3);
  stream << 100 << 200 << 300;

  auto bytes = stream.out();
  dev::RLP rlp(bytes);

  EXPECT_TRUE(rlp.isList());
  EXPECT_EQ(rlp.itemCount(), 3);
  EXPECT_EQ(rlp[0].toInt<int>(), 100);
  EXPECT_EQ(rlp[1].toInt<int>(), 200);
  EXPECT_EQ(rlp[2].toInt<int>(), 300);
}

TEST_F(EncodingTest, rlp_from_string) {
  // Test RLP construction from string
  dev::RLPStream stream;
  stream.appendList(2);
  stream << "hello" << "world";

  auto bytes = stream.out();
  std::string str(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  dev::RLP rlp(str);

  EXPECT_TRUE(rlp.isList());
  EXPECT_EQ(rlp.itemCount(), 2);
  EXPECT_EQ(rlp[0].toString(), "hello");
  EXPECT_EQ(rlp[1].toString(), "world");
}

TEST_F(EncodingTest, rlp_from_pointer) {
  // Test RLP construction from raw pointer (zero-copy pattern)
  dev::RLPStream stream;
  stream.appendList(4);
  stream << 1 << 2 << 3 << 4;

  auto bytes = stream.out();
  // This is the zero-copy pattern used in DbStorage::sliceToRlp
  dev::RLP rlp(reinterpret_cast<::byte const*>(bytes.data()), bytes.size());

  EXPECT_TRUE(rlp.isList());
  EXPECT_EQ(rlp.itemCount(), 4);
}

TEST_F(EncodingTest, rlp_nested_lists) {
  // Test RLP with nested lists
  dev::RLPStream stream;
  stream.appendList(2);
  stream.appendList(2);
  stream << 1 << 2;
  stream.appendList(3);
  stream << 3 << 4 << 5;

  auto bytes = stream.out();
  dev::RLP rlp(bytes);

  EXPECT_TRUE(rlp.isList());
  EXPECT_EQ(rlp.itemCount(), 2);

  auto first = rlp[0];
  EXPECT_TRUE(first.isList());
  EXPECT_EQ(first.itemCount(), 2);
  EXPECT_EQ(first[0].toInt<int>(), 1);
  EXPECT_EQ(first[1].toInt<int>(), 2);

  auto second = rlp[1];
  EXPECT_TRUE(second.isList());
  EXPECT_EQ(second.itemCount(), 3);
  EXPECT_EQ(second[0].toInt<int>(), 3);
  EXPECT_EQ(second[1].toInt<int>(), 4);
  EXPECT_EQ(second[2].toInt<int>(), 5);
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
