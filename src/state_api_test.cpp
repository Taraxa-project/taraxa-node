#include "state_api.hpp"

#include <boost/filesystem.hpp>
#include <fstream>
#include <optional>
#include <vector>

#include "util/encoding_rlp.hpp"
#include "util_test/gtest.hpp"

namespace taraxa::state_api {
using boost::filesystem::create_directories;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using util::dec_rlp;
using namespace std;

struct StateAPITest : WithDataDir {};

static auto const base_taraxa_chain_cfg = [] {
  ChainConfig ret;
  ret.disable_block_rewards = true;
  ret.execution_options.disable_nonce_check = true;
  ret.execution_options.disable_gas_fee = true;
  ret.eth_chain_config.dao_fork_block = BlockNumberNIL;
  return ret;
}();

struct TestBlock {
  h256 Hash;
  h256 StateRoot;
  EVMBlock evm_block;
  vector<EVMTransaction> Transactions;
  vector<UncleBlock> UncleBlocks;
};

void dec_rlp(RLP const& rlp, TestBlock& target) {
  dec_rlp_tuple(rlp, target.Hash, target.StateRoot, target.evm_block, target.Transactions, target.UncleBlocks);
}

template <typename T>
T parse_rlp_file(path const& p) {
  ifstream strm(p.string());
  T ret;
  dec_rlp(RLP(string(istreambuf_iterator(strm), {}), 0), ret);
  return ret;
}

TEST_F(StateAPITest, eth_mainnet_smoke) {
  auto test_blocks = parse_rlp_file<vector<TestBlock>>(path(__FILE__).parent_path().parent_path() / "submodules" / "taraxa-evm" / "taraxa" / "data" /
                                                       "eth_mainnet_blocks_0_300000.rlp");

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
  auto genesis_balances_rlp = dev::jsToBytes(string((char*)genesis_balances_rlp_hex_c.Data, genesis_balances_rlp_hex_c.Len));
  dec_rlp(RLP(genesis_balances_rlp), chain_config.genesis_balances);

  Opts opts;
  opts.ExpectedMaxTrxPerBlock = 300;
  opts.MainTrieFullNodeLevelsToCache = 4;

  StateAPI SUT((data_dir / "state").string(), [&](auto n) { return test_blocks[n].Hash; },  //
               chain_config, opts);

  ASSERT_EQ(test_blocks[0].StateRoot, SUT.get_last_committed_state_descriptor().state_root);
  auto progress_pct = numeric_limits<int>::min();
  for (size_t blk_num = 1; blk_num < test_blocks.size(); ++blk_num) {
    if (int n = 100 * blk_num / test_blocks.size(); n >= progress_pct + 10) {
      // I'm aware about \r and flush(), but it doesn't always work
      cout << "progress: " << (progress_pct = n) << "%" << endl;
    }
    auto const& test_block = test_blocks[blk_num];
    auto const& result = SUT.transition_state(test_block.evm_block, test_block.Transactions, test_block.UncleBlocks);
    ASSERT_EQ(result.StateRoot, test_block.StateRoot);
    SUT.transition_state_commit();
  }
}

TEST_F(StateAPITest, dpos_integration) {
  auto chain_cfg = base_taraxa_chain_cfg;

  auto addr_1 = addr_t(1);
  auto addr_2 = addr_t(2);
  auto addr_3 = addr_t(3);

  u256 addr_1_bal_expected = 100000000;
  chain_cfg.genesis_balances[addr_1] = addr_1_bal_expected;
  auto& dpos_cfg = chain_cfg.dpos.emplace();
  dpos_cfg.deposit_delay = 2;
  dpos_cfg.withdrawal_delay = 4;
  dpos_cfg.eligibility_balance_threshold = 1000;
  addr_1_bal_expected -= dpos_cfg.genesis_state[addr_1][addr_1] = dpos_cfg.eligibility_balance_threshold;

  uint64_t curr_blk = 0;
  StateAPI SUT((data_dir / "state").string(), [&](auto n) -> h256 { assert(false); },  //
               chain_cfg);

  vector<addr_t> expected_eligible_set;
  auto CHECK = [&] {
    string meta = "at block " + to_string(curr_blk);
    EXPECT_EQ(addr_1_bal_expected, SUT.get_account(curr_blk, addr_1)->Balance) << meta;
    for (auto const& addr : expected_eligible_set) {
      EXPECT_TRUE(SUT.dpos_is_eligible(curr_blk, addr)) << meta;
    }
    EXPECT_EQ(SUT.dpos_eligible_count(curr_blk), expected_eligible_set.size()) << meta;
  };
  auto EXEC_AND_CHECK = [&](vector<EVMTransaction> const& trxs) {
    auto result = SUT.transition_state({}, trxs);
    SUT.transition_state_commit();
    for (auto& r : result.ExecutionResults) {
      EXPECT_TRUE(r.CodeRet.empty());
      EXPECT_TRUE(r.CodeErr.empty());
    }
    ++curr_blk;
    CHECK();
  };

  DPOSTransfers transfers;
  auto make_dpos_trx = [&] {
    StateAPI::DPOSTransactionPrototype trx_proto(transfers);
    transfers = {};
    EVMTransaction trx;
    trx.From = addr_1;
    trx.To = trx_proto.to;
    trx.Value = trx_proto.value;
    trx.Input = trx_proto.input;
    trx.Gas = trx_proto.minimal_gas;
    return trx;
  };

  expected_eligible_set = {addr_1};
  CHECK();

  addr_1_bal_expected -= transfers[addr_2].value = dpos_cfg.eligibility_balance_threshold;
  addr_1_bal_expected -= transfers[addr_3].value = dpos_cfg.eligibility_balance_threshold - 1;
  EXEC_AND_CHECK({make_dpos_trx()});

  u256 withdrawal_val = 1;
  transfers[addr_2] = {withdrawal_val, true};
  addr_1_bal_expected -= transfers[addr_3].value = 1;
  EXEC_AND_CHECK({make_dpos_trx()});

  expected_eligible_set = {addr_1, addr_2};
  EXEC_AND_CHECK({});

  expected_eligible_set = {addr_1, addr_2, addr_3};
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});

  addr_1_bal_expected += withdrawal_val;
  expected_eligible_set = {addr_1, addr_3};
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
}

}  // namespace taraxa::state_api

TARAXA_TEST_MAIN({})
