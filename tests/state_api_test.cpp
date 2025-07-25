#include "final_chain/state_api.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>

#include "common/encoding_rlp.hpp"
#include "slashing_manager/slashing_manager.hpp"
#include "test_util/test_util.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::state_api {
using boost::filesystem::create_directories;
using boost::filesystem::path;
using boost::filesystem::remove_all;
using boost::filesystem::temp_directory_path;
using namespace std;
// using namespace core_tests;

struct StateAPITest : NodesTest {};

struct TestBlock {
  h256 hash;
  h256 state_root;
  EVMBlock evm_block;
  vector<EVMTransaction> transactions;

  RLP_FIELDS_DEFINE_INPLACE(hash, state_root, evm_block, transactions)
};

template <typename T>
T parse_rlp_file(path const& p) {
  ifstream strm(p.string());
  T ret;
  util::rlp(dev::RLP(string(istreambuf_iterator(strm), {}), 0), ret);
  return ret;
}

TEST_F(StateAPITest, DISABLED_dpos_integration) {
  // Config chain_cfg;

  // DPOSQuery::AccountQuery acc_q;
  // acc_q.with_staking_balance = true;
  // acc_q.with_outbound_deposits = true;
  // acc_q.with_inbound_deposits = true;
  // DPOSQuery q;
  // q.with_eligible_count = true;
  // q.account_queries[make_addr(1)] = acc_q;
  // q.account_queries[make_addr(2)] = acc_q;
  // q.account_queries[make_addr(3)] = acc_q;

  // u256 addr_1_bal_expected = 100000000;
  // chain_cfg.initial_balances[make_addr(1)] = addr_1_bal_expected;
  // auto& dpos_cfg = chain_cfg.dpos.emplace();
  // dpos_cfg.delegation_delay = 2;
  // dpos_cfg.delegation_locking_period = 4;
  // dpos_cfg.eligibility_balance_threshold = 1000;
  // dpos_cfg.vote_eligibility_balance_step = 1000;
  // addr_1_bal_expected -= dpos_cfg.genesis_state[make_addr(1)][make_addr(1)] = dpos_cfg.eligibility_balance_threshold;

  // uint64_t curr_blk = 0;
  // StateAPI SUT([&](auto /*n*/) -> h256 { assert(false); },  //
  //              chain_cfg,
  //              {
  //                  10,
  //                  1,
  //              },
  //              {
  //                  (data_dir / "state").string(),
  //              });

  // unordered_set<addr_t> expected_eligible_set;
  // decltype(DPOSQueryResult().account_results) exp_q_acc_res;
  // auto CHECK = [&] {
  //   for (auto& [addr, res] : exp_q_acc_res) {
  //     res.is_eligible = expected_eligible_set.count(addr);
  //   }
  //   for (auto const& addr : expected_eligible_set) {
  //     exp_q_acc_res[addr].is_eligible = true;
  //   }
  //   string meta = "at block " + to_string(curr_blk);
  //   EXPECT_EQ(addr_1_bal_expected, SUT.getAccount(curr_blk, make_addr(1))->balance) << meta;
  //   for (auto const& addr : expected_eligible_set) {
  //     EXPECT_TRUE(SUT.dposIsEligible(curr_blk, addr)) << meta;
  //     EXPECT_EQ(SUT.dposEligibleVoteCount(curr_blk, addr), 1) << meta;
  //   }
  //   EXPECT_EQ(SUT.dposEligibleTotalVoteCount(curr_blk), expected_eligible_set.size()) << meta;
  //   // auto q_res = SUT.dpos_query(curr_blk, q);
  //   EXPECT_EQ(q_res.eligible_count, expected_eligible_set.size()) << meta;
  //   for (auto& [addr, res_exp] : exp_q_acc_res) {
  //     auto& res_act = q_res.account_results[addr];
  //     auto meta_ = meta + " @ " + addr.hex();
  //     EXPECT_EQ(res_exp.staking_balance, res_act.staking_balance) << meta_;
  //     EXPECT_EQ(res_exp.is_eligible, res_act.is_eligible) << meta_;
  //     for (auto [label, deposits_p_exp, deposits_p_act] : {
  //              tuple{"in", &res_exp.inbound_deposits, &res_act.inbound_deposits},
  //              tuple{"out", &res_exp.outbound_deposits, &res_act.outbound_deposits},
  //          }) {
  //       auto& deposits_exp = *deposits_p_exp;
  //       auto& deposits_act = *deposits_p_act;
  //       auto meta__ = meta_ + " (" + label + ")";
  //       EXPECT_EQ(deposits_exp.size(), deposits_act.size()) << meta__;
  //       for (auto& [addr, deposit_v_exp] : deposits_exp) {
  //         EXPECT_EQ(deposit_v_exp, deposits_act[addr]) << meta__;
  //       }
  //     }
  //   }
  // };
  // auto EXEC_AND_CHECK = [&](vector<EVMTransaction> const& trxs) {
  //   auto result = SUT.transition_state({}, trxs);
  //   SUT.transition_state_commit();
  //   for (auto& r : result.execution_results) {
  //     EXPECT_TRUE(r.code_retval.empty());
  //     EXPECT_TRUE(r.code_err.empty());
  //   }
  //   ++curr_blk;
  //   CHECK();
  // };

  // DPOSTransfers transfers;
  // auto make_dpos_trx = [&] {
  //   StateAPI::DPOSTransactionPrototype trx_proto(transfers);
  //   transfers = {};
  //   EVMTransaction trx;
  //   trx.from = make_addr(1);
  //   trx.to = trx_proto.to;
  //   trx.value = trx_proto.value;
  //   trx.input = trx_proto.input;
  //   trx.gas = trx_proto.minimal_gas;
  //   return trx;
  // };

  // expected_eligible_set = {make_addr(1)};
  // exp_q_acc_res[make_addr(1)].inbound_deposits[make_addr(1)] =
  //     exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(1)].staking_balance =
  //     1000;
  // CHECK();

  // addr_1_bal_expected -= transfers[make_addr(2)].value = 1000;
  // addr_1_bal_expected -= transfers[make_addr(3)].value = 999;
  // EXEC_AND_CHECK({make_dpos_trx()});

  // transfers[make_addr(2)] = {1, true};
  // addr_1_bal_expected -= transfers[make_addr(3)].value = 1;
  // EXEC_AND_CHECK({make_dpos_trx()});

  // expected_eligible_set = {make_addr(1), make_addr(2)};
  // exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(2)] =
  //     exp_q_acc_res[make_addr(2)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(2)].staking_balance =
  //     1000;
  // exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(3)] =
  //     exp_q_acc_res[make_addr(3)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(3)].staking_balance = 999;
  // EXEC_AND_CHECK({});

  // expected_eligible_set = {make_addr(1), make_addr(2), make_addr(3)};
  // exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(3)] =
  //     exp_q_acc_res[make_addr(3)].inbound_deposits[make_addr(1)] = 1000;
  // exp_q_acc_res[make_addr(3)].staking_balance = 1000;
  // EXEC_AND_CHECK({});
  // EXEC_AND_CHECK({});

  // addr_1_bal_expected += 1;
  // expected_eligible_set = {make_addr(1), make_addr(3)};
  // exp_q_acc_res[make_addr(1)].outbound_deposits[make_addr(2)] =
  //     exp_q_acc_res[make_addr(2)].inbound_deposits[make_addr(1)] = exp_q_acc_res[make_addr(2)].staking_balance = 999;
  // EXEC_AND_CHECK({});
  // EXEC_AND_CHECK({});
  // EXEC_AND_CHECK({});
  // EXEC_AND_CHECK({});
}

TEST_F(StateAPITest, DISABLED_eth_mainnet_smoke) {
  auto test_blocks =
      parse_rlp_file<vector<TestBlock>>(path(__FILE__).parent_path().parent_path() / "submodules" / "taraxa-evm" /
                                        "taraxa" / "data" / "eth_mainnet_blocks_0_300000.rlp");

  Config chain_config;
  auto initial_balances_rlp_hex_c = taraxa_evm_mainnet_initial_balances();
  auto initial_balances_rlp =
      dev::jsToBytes(string((char*)initial_balances_rlp_hex_c.Data, initial_balances_rlp_hex_c.Len));
  util::rlp(dev::RLP(initial_balances_rlp), chain_config.initial_balances);

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
    SUT.execute_transactions(test_block.evm_block, test_block.transactions);
    const auto& result = SUT.distribute_rewards({});
    ASSERT_EQ(result.state_root, test_block.state_root);
    SUT.transition_state_commit();
  }
}

TEST_F(StateAPITest, slashing) {
  auto node_cfgs = make_node_cfgs(1, 1, 5);
  // Option 2: more sophisticated and longer test
  // auto node_cfgs = make_node_cfgs(4, 4, 5);
  for (auto& cfg : node_cfgs) {
    cfg.genesis.state.dpos.delegation_delay = 2;
    cfg.genesis.state.hardforks.magnolia_hf.jail_time = 2;
    cfg.genesis.state.hardforks.magnolia_hf.block_num = 0;
    cfg.report_malicious_behaviour = true;
  }

  auto nodes = launch_nodes(node_cfgs);
  auto node = nodes.begin()->get();
  auto node_cfg = node_cfgs.begin();
  ASSERT_EQ(true, node->getFinalChain()->dposIsEligible(node->getFinalChain()->lastBlockNumber(), node->getAddress()));

  // Generate 2 cert votes for 2 different blocks
  auto vote_a = node->getVoteManager()->generateVote(blk_hash_t{1}, PbftVoteTypes::cert_vote, 1, 1, 3);
  auto vote_b = node->getVoteManager()->generateVote(blk_hash_t{2}, PbftVoteTypes::cert_vote, 1, 1, 3);

  // Commit double voting proof
  auto slashing_manager = std::make_shared<SlashingManager>(*node_cfg, node->getFinalChain(),
                                                            node->getTransactionManager(), node->getGasPricer());
  ASSERT_EQ(true, slashing_manager->submitDoubleVotingProof(vote_a, vote_b));

  // After few blocks malicious validator should be jailed
  ASSERT_HAPPENS({10s, 100ms}, [&](auto& ctx) {
    WAIT_EXPECT_EQ(ctx, false,
                   node->getFinalChain()->dposIsEligible(node->getFinalChain()->lastBlockNumber(), node->getAddress()))
  });

  // Option 2: more sophisticated and longer test
  // After few blocks malicious validator should be unjailed
  //  ASSERT_HAPPENS({5s, 100ms}, [&](auto& ctx) {
  //    WAIT_EXPECT_EQ(
  //        ctx, true,
  //        node->getFinalChain()->dposIsEligible(node->getFinalChain()->lastBlockNumber(), node->getAddress()))
  //  });
}

}  // namespace taraxa::state_api

TARAXA_TEST_MAIN({})
