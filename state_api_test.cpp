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

struct StateAPITest : testing::Test, WithTestDataDir {};

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
  auto test_blocks = parse_rlp_file<vector<TestBlock>>(
      test_data_dir / "eth_mainnet_blocks_0_300000.rlp");

  ChainConfig chain_config;
  auto& eth_cfg = chain_config.eth_chain_config;
  eth_cfg.homestead_block = 1150000;
  eth_cfg.dao_fork_block = 1920000;
  eth_cfg.eip_150_block = 2463000;
  eth_cfg.eip_158_block = 2675000;
  eth_cfg.byzantium_block = 4370000;
  eth_cfg.constantinople_block = 7280000;
  eth_cfg.petersburg_block = 7280000;

  auto genesis_balances_rlp_hex_c = taraxa_evm_mainnet_genesis_balances();
  auto genesis_balances_rlp = dev::jsToBytes(string(
      (char*)genesis_balances_rlp_hex_c.Data, genesis_balances_rlp_hex_c.Len));
  dec_rlp(RLP(genesis_balances_rlp), chain_config.genesis_balances);

  Opts opts;
  opts.ExpectedMaxTrxPerBlock = 300;
  opts.MainTrieFullNodeLevelsToCache = 4;

  StateAPI api((data_dir / "state").string(),
               [&](auto n) { return test_blocks[n].Hash; },  //
               chain_config, opts);

  ASSERT_EQ(test_blocks[0].StateRoot,
            api.get_last_committed_state_descriptor().state_root);
  auto progress_pct = numeric_limits<int>::min();
  for (size_t blk_num = 1; blk_num < test_blocks.size(); ++blk_num) {
    if (int n = 100 * blk_num / test_blocks.size(); n >= progress_pct + 10) {
      // I'm aware about \r and flush(), but it doesn't always work
      cout << "progress: " << (progress_pct = n) << "%" << endl;
    }
    auto const& test_block = test_blocks[blk_num];
    auto const& result = api.transition_state(
        test_block.evm_block, test_block.Transactions, test_block.UncleBlocks);
    ASSERT_EQ(result.StateRoot, test_block.StateRoot);
    api.transition_state_commit();
  }
}

}  // namespace taraxa::state_api

TARAXA_TEST_MAIN({})
