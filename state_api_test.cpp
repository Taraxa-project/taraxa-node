#include "state_api.hpp"

#include <boost/filesystem.hpp>
#include <fstream>
#include <optional>
#include <vector>

#include "util/encoding_rlp.hpp"
#include "util/gtest.hpp"

namespace taraxa::state_api {
using boost::filesystem::create_directories;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using util::dec_rlp;
using namespace std;

struct StateAPITest : testing::Test, WithTestDataDir {
  shared_ptr<DbStorage> db = DbStorage::make(data_dir, h256::random(), true);
};

struct TestBlock {
  h256 Hash;
  h256 StateRoot;
  EVMBlock evm_block;
  vector<EVMTransaction> Transactions;
  vector<UncleBlock> UncleBlocks;
};

void dec_rlp(RLP const& rlp, TestBlock& target) {
  dec_rlp_tuple(rlp, target.Hash, target.StateRoot, target.evm_block,
                target.Transactions, target.UncleBlocks);
}

template <typename T>
T parse_rlp_file(path const& p) {
  ifstream strm(p.string());
  T ret;
  dec_rlp(RLP(string(istreambuf_iterator(strm), {}), 0), ret);
  return ret;
}

TEST_F(StateAPITest, eth_mainnet_smoke) {
  static auto const test_data_dir = path(__FILE__).parent_path() /
                                    "submodules" / "taraxa-evm" / "taraxa" /
                                    "data";
  auto genesis_accs = parse_rlp_file<InputAccounts>(
      test_data_dir / "eth_mainnet_genesis_accounts.rlp");
  auto test_blocks = parse_rlp_file<vector<TestBlock>>(
      test_data_dir / "eth_mainnet_blocks_0_300000.rlp");

  ChainConfig chain_config;
  auto& eth_chain_cfg = chain_config.evm_chain_config.eth_chain_config;
  eth_chain_cfg.homestead_block = 1150000;
  eth_chain_cfg.dao_fork_block = 1920000;
  eth_chain_cfg.eip_150_block = 2463000;
  eth_chain_cfg.eip_158_block = 2675000;
  eth_chain_cfg.byzantium_block = 4370000;
  eth_chain_cfg.constantinople_block = 7280000;
  eth_chain_cfg.petersburg_block = 7280000;

  CacheOpts cache_opts;
  cache_opts.ExpectedMaxNumTrxPerBlock = 200;
  cache_opts.MainTrieWriterOpts.ExpectedDepth = 64;  // TODO constant
  cache_opts.MainTrieWriterOpts.FullNodeLevelsToCache = 5;
  cache_opts.AccTrieWriterOpts.ExpectedDepth = 16;

  StateAPI SUT(
      *db, [&](auto n) { return test_blocks[n].Hash; },  //
      chain_config, 0, {}, cache_opts);

  auto batch = db->createWriteBatch();
  auto state_root = SUT.StateTransition_ApplyAccounts(*batch, genesis_accs);
  ASSERT_EQ(test_blocks[0].StateRoot, state_root);
  db->commitWriteBatch(batch);
  SUT.NotifyStateTransitionCommitted();
  auto progress_pct = numeric_limits<int>::min();
  for (size_t blk_num = 1; blk_num < test_blocks.size(); ++blk_num) {
    if (int n = 100 * blk_num / test_blocks.size(); n >= progress_pct + 10) {
      // I'm aware about \r and flush(), but it doesn't always work
      cout << "progress: " << (progress_pct = n) << "%" << endl;
    }
    auto const& test_block = test_blocks[blk_num];
    auto batch = db->createWriteBatch();
    auto const& result = SUT.StateTransition_ApplyBlock(
        *batch, test_block.evm_block, test_block.Transactions,
        test_block.UncleBlocks, {});
    ASSERT_EQ(result.StateRoot, test_block.StateRoot);
    db->commitWriteBatch(batch);
    SUT.NotifyStateTransitionCommitted();
  }
}

}  // namespace taraxa::state_api

TARAXA_TEST_MAIN({})
