#include "final_chain.hpp"

#include <boost/filesystem.hpp>
#include <fstream>
#include <optional>
#include <vector>

#include "chain_config.hpp"
#include "util/gtest.hpp"

namespace taraxa::final_chain {
using boost::filesystem::create_directories;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using namespace std;

struct FinalChainTest : testing::Test, WithTestDataDir {
  shared_ptr<DbStorage> db = DbStorage::make(data_dir, h256::random(), true);
};

TEST_F(FinalChainTest, test_1) {
  auto cfg = ChainConfig::Default->final_chain;
  auto sender_keys = KeyPair::create();
  auto sender_addr = sender_keys.address();
  auto sender_init_bal = 1000;
  cfg.state.genesis_accounts = {};
  cfg.state.genesis_accounts[sender_addr].Balance = sender_init_bal;
  FinalChain SUT(db, cfg);
  auto acc = SUT.get_state_api()->Historical_GetAccount(0, sender_addr);
  ASSERT_EQ(acc->Balance, sender_init_bal);
  auto genesis_block = SUT.get_block_db()->blockHeader(0);
  auto batch = db->createWriteBatch();
  uint64_t timestamp = 1000;
  auto val = 100;
  auto receiver = addr_t::random();
  dev::eth::Transaction trx(val, 0, 19000, receiver, {}, 0,
                            sender_keys.secret());
  auto result = SUT.advance(batch, sender_addr, timestamp, vector{trx});
  SUT.advance_confirm();
  ASSERT_EQ(result.new_header.author(), sender_addr);
  ASSERT_EQ(result.new_header.timestamp(), timestamp);
  ASSERT_EQ(result.new_header.number(), 1);
  ASSERT_EQ(result.new_header.parentHash(), genesis_block.hash());
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
