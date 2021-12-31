#include "final_chain/state_api.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <optional>
#include <vector>

#include "common/encoding_rlp.hpp"
#include "util_test/util.hpp"

namespace taraxa::state_api {
using boost::filesystem::create_directories;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using namespace std;
using namespace core_tests;

struct StateAPITest : WithDataDir {};

static auto const base_taraxa_chain_cfg = [] {
  Config ret;
  ret.disable_block_rewards = true;
  ret.execution_options.disable_nonce_check = true;
  ret.execution_options.disable_gas_fee = true;
  ret.eth_chain_config.dao_fork_block = BlockNumberNIL;
  return ret;
}();

struct TestBlock {
  h256 hash;
  h256 state_root;
  EVMBlock evm_block;
  vector<EVMTransaction> transactions;
  vector<UncleBlock> uncle_blocks;

  RLP_FIELDS_DEFINE_INPLACE(hash, state_root, evm_block, transactions, uncle_blocks)
};

template <typename T>
T parse_rlp_file(path const& p) {
  ifstream strm(p.string());
  T ret;
  util::rlp(dev::RLP(string(istreambuf_iterator(strm), {}), 0), ret);
  return ret;
}

TEST_F(StateAPITest, dpos_integration) {
  auto chain_cfg = base_taraxa_chain_cfg;

  DPOSQuery::AccountQuery acc_q;
  acc_q.with_staking_balance = true;
  acc_q.with_outbound_deposits = true;
  acc_q.with_inbound_deposits = true;
  DPOSQuery q;
  q.with_eligible_count = true;
  q.account_queries[make_addr(1)] = acc_q;
  q.account_queries[make_addr(2)] = acc_q;
  q.account_queries[make_addr(3)] = acc_q;

  u256 addr_1_bal_expected = 100000000;
  chain_cfg.genesis_balances[make_addr(1)] = addr_1_bal_expected;
  auto& dpos_cfg = chain_cfg.dpos.emplace();
  dpos_cfg.deposit_delay = 2;
  dpos_cfg.withdrawal_delay = 4;
  dpos_cfg.eligibility_balance_threshold = 1000;
  addr_1_bal_expected -= dpos_cfg.genesis_state[make_addr(1)][make_addr(1)] = dpos_cfg.eligibility_balance_threshold;

  uint64_t curr_blk = 0;
  StateAPI SUT([&](auto /*n*/) -> h256 { assert(false); },  //
               chain_cfg,
               {
                   10,
                   1,
               },
               {
                   (data_dir / "state").string(),
               });

  unordered_set<addr_t> expected_eligible_set;
  decltype(DPOSQueryResult().account_results) exp_q_acc_res;
  auto CHECK = [&] {
    for (auto& [addr, res] : exp_q_acc_res) {
      res.is_eligible = expected_eligible_set.count(addr);
    }
    for (auto const& addr : expected_eligible_set) {
      exp_q_acc_res[addr].is_eligible = true;
    }
    string meta = "at block " + to_string(curr_blk);
    EXPECT_EQ(addr_1_bal_expected, SUT.get_account(curr_blk, make_addr(1))->balance) << meta;
    for (auto const& addr : expected_eligible_set) {
      EXPECT_TRUE(SUT.dpos_is_eligible(curr_blk, addr)) << meta;
      EXPECT_EQ(SUT.dpos_eligible_vote_count(curr_blk, addr), 1) << meta;
    }
    EXPECT_EQ(SUT.dpos_eligible_count(curr_blk), expected_eligible_set.size()) << meta;
    EXPECT_EQ(SUT.dpos_eligible_total_vote_count(curr_blk), expected_eligible_set.size()) << meta;
    auto q_res = SUT.dpos_query(curr_blk, q);
    EXPECT_EQ(q_res.eligible_count, expected_eligible_set.size()) << meta;
    for (auto& [addr, res_exp] : exp_q_acc_res) {
      auto& res_act = q_res.account_results[addr];
      auto meta_ = meta + " @ " + addr.hex();
      EXPECT_EQ(res_exp.staking_balance, res_act.staking_balance) << meta_;
      EXPECT_EQ(res_exp.is_eligible, res_act.is_eligible) << meta_;
      for (auto [label, deposits_p_exp, deposits_p_act] : {
               tuple{"in", &res_exp.inbound_deposits, &res_act.inbound_deposits},
               tuple{"out", &res_exp.outbound_deposits, &res_act.outbound_deposits},
           }) {
        auto& deposits_exp = *deposits_p_exp;
        auto& deposits_act = *deposits_p_act;
        auto meta__ = meta_ + " (" + label + ")";
        EXPECT_EQ(deposits_exp.size(), deposits_act.size()) << meta__;
        for (auto& [addr, deposit_v_exp] : deposits_exp) {
          EXPECT_EQ(deposit_v_exp, deposits_act[addr]) << meta__;
        }
      }
    }
  };
  auto EXEC_AND_CHECK = [&](vector<EVMTransaction> const& trxs) {
    auto result = SUT.transition_state({}, trxs);
    SUT.transition_state_commit();
    for (auto& r : result.execution_results) {
      EXPECT_TRUE(r.code_retval.empty());
      EXPECT_TRUE(r.code_err.empty());
    }
    ++curr_blk;
    CHECK();
  };

  DPOSTransfers transfers;
  auto make_dpos_trx = [&] {
    StateAPI::DPOSTransactionPrototype trx_proto(transfers);
    transfers = {};
    EVMTransaction trx;
    trx.from = make_addr(1);
    trx.to = trx_proto.to;
    trx.value = trx_proto.value;
    trx.input = trx_proto.input;
    trx.gas = trx_proto.minimal_gas;
    return trx;
  };

  expected_eligible_set = {make_addr(1)};
  exp_q_acc_res[make_addr(1)].inbound_deposits[make_addr(1)] =
      exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(1)].staking_balance = 1000;
  CHECK();

  addr_1_bal_expected -= transfers[make_addr(2)].value = 1000;
  addr_1_bal_expected -= transfers[make_addr(3)].value = 999;
  EXEC_AND_CHECK({make_dpos_trx()});

  transfers[make_addr(2)] = {1, true};
  addr_1_bal_expected -= transfers[make_addr(3)].value = 1;
  EXEC_AND_CHECK({make_dpos_trx()});

  expected_eligible_set = {make_addr(1), make_addr(2)};
  exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(2)] =
      exp_q_acc_res[make_addr(2)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(2)].staking_balance = 1000;
  exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(3)] =
      exp_q_acc_res[make_addr(3)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(3)].staking_balance = 999;
  EXEC_AND_CHECK({});

  expected_eligible_set = {make_addr(1), make_addr(2), make_addr(3)};
  exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(3)] =
      exp_q_acc_res[make_addr(3)].inbound_deposits[make_addr(1)] = 1000;
  exp_q_acc_res[make_addr(3)].staking_balance = 1000;
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});

  addr_1_bal_expected += 1;
  expected_eligible_set = {make_addr(1), make_addr(3)};
  exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(2)] =
      exp_q_acc_res[make_addr(2)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(2)].staking_balance = 999;
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
  EXEC_AND_CHECK({});
}

// disabled because of changing balances hardfork
TEST_F(StateAPITest, DISABLED_eth_mainnet_smoke) {
  auto test_blocks =
      parse_rlp_file<vector<TestBlock>>(path(__FILE__).parent_path().parent_path() / "submodules" / "taraxa-evm" /
                                        "taraxa" / "data" / "eth_mainnet_blocks_0_300000.rlp");

  Config chain_config;
  auto& eth_cfg = chain_config.eth_chain_config;
  eth_cfg.homestead_block = 1150000;
  eth_cfg.dao_fork_block = 1920000;
  eth_cfg.eip_150_block = 2463000;
  eth_cfg.eip_158_block = 2675000;
  eth_cfg.byzantium_block = 4370000;
  eth_cfg.constantinople_block = 7280000;
  eth_cfg.petersburg_block = 7280000;

  auto genesis_balances_rlp_hex_c = taraxa_evm_mainnet_genesis_balances();
  auto genesis_balances_rlp =
      dev::jsToBytes(string((char*)genesis_balances_rlp_hex_c.Data, genesis_balances_rlp_hex_c.Len));
  util::rlp(dev::RLP(genesis_balances_rlp), chain_config.genesis_balances);

  Opts opts;
  opts.expected_max_trx_per_block = 300;
  opts.max_trie_full_node_levels_to_cache = 4;

  StateAPI SUT([&](auto n) { return test_blocks[n].hash; },  //
               chain_config, opts,
               {
                   (data_dir / "state").string(),
               });

  ASSERT_EQ(test_blocks[0].state_root, SUT.get_last_committed_state_descriptor().state_root);
  size_t num_blk_to_exec = 150000;  // test_blocks.size() will provide more coverage but will be slower
  long double progress_pct = 0, progress_pct_log_threshold = 0;
  auto one_blk_in_pct = (long double)100 / num_blk_to_exec;
  for (size_t blk_num = 1; blk_num < num_blk_to_exec; ++blk_num) {
    if ((progress_pct += one_blk_in_pct) >= progress_pct_log_threshold) {
      cout << "progress: " << uint(progress_pct) << "%" << endl;
      progress_pct_log_threshold += 10;
    }
    auto const& test_block = test_blocks[blk_num];
    auto const& result = SUT.transition_state(test_block.evm_block, test_block.transactions, test_block.uncle_blocks);
    ASSERT_EQ(result.state_root, test_block.state_root);
    SUT.transition_state_commit();
  }
}

TEST_F(StateAPITest, increase_balance_hardfork_test) {
  const auto hardfork_block_num = 12000;
  Config chain_config;
  chain_config.genesis_balances.emplace(addr_t(1111), 1000);
  chain_config.genesis_balances.emplace(addr_t(1234), 10000);

  Opts opts;
  opts.expected_max_trx_per_block = 300;
  opts.max_trie_full_node_levels_to_cache = 4;

  StateAPI SUT([&](auto) { return h256(); },  //
               chain_config, opts,
               {
                   (data_dir / "state").string(),
               });
  for (uint16_t i = 0; i < hardfork_block_num; ++i) {
    SUT.transition_state({}, {}, {});
    SUT.transition_state_commit();
  }

  const auto mult = u256(1e18);
  for (const auto& b : chain_config.genesis_balances) {
    const auto balance_after = SUT.get_account(hardfork_block_num, b.first)->balance;
    EXPECT_EQ(b.second * mult, balance_after);
  }
}

}  // namespace taraxa::state_api

TARAXA_TEST_MAIN({})
