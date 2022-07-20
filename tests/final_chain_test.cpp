#include "final_chain/final_chain.hpp"

#include <optional>
#include <vector>

#include "common/constants.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/chain_config.hpp"
#include "final_chain/trie_common.hpp"
#include "util_test/gtest.hpp"
#include "util_test/samples.hpp"
#include "util_test/util.hpp"
#include "vote/vote.hpp"

namespace taraxa::final_chain {
using namespace taraxa::core_tests;

struct advance_check_opts {
  bool dont_assume_no_logs = 0;
  bool dont_assume_all_trx_success = 0;
  bool expect_to_fail = 0;
};

struct FinalChainTest : WithDataDir {
  std::shared_ptr<DbStorage> db{new DbStorage(data_dir / "db")};
  ChainConfig cfg = ChainConfig::predefined("test");
  std::shared_ptr<FinalChain> SUT;
  bool assume_only_toplevel_transfers = true;
  std::unordered_map<addr_t, u256> expected_balances;
  uint64_t expected_blk_num = 0;

  void init() {
    SUT = NewFinalChain(db, cfg);
    const auto& effective_balances = effective_genesis_balances(cfg.final_chain.state);
    for (auto const& [addr, _] : cfg.final_chain.state.genesis_balances) {
      auto acc_actual = SUT->get_account(addr);
      ASSERT_TRUE(acc_actual);
      const auto expected_bal = effective_balances.at(addr);
      ASSERT_EQ(acc_actual->balance, expected_bal);
      expected_balances[addr] = expected_bal;
    }
  }

  auto advance(SharedTransactions const& trxs, advance_check_opts opts = {}) {
    SUT = nullptr;
    SUT = NewFinalChain(db, cfg);
    std::vector<h256> trx_hashes;
    int pos = 0;
    for (auto const& trx : trxs) {
      db->saveTransactionPeriod(trx->getHash(), 1, pos++);
      trx_hashes.emplace_back(trx->getHash());
    }
    DagBlock dag_blk({}, {}, {}, trx_hashes, {}, {}, secret_t::random());
    db->saveDagBlock(dag_blk);
    std::vector<vote_hash_t> reward_votes_hashes;
    auto pbft_block = std::make_shared<PbftBlock>(blk_hash_t(), blk_hash_t(), blk_hash_t(), 1, addr_t::random(),
                                                  dev::KeyPair::create().secret(), std::move(reward_votes_hashes));
    std::vector<std::shared_ptr<Vote>> votes;
    PeriodData period_data(pbft_block, votes);
    period_data.dag_blocks.push_back(dag_blk);
    period_data.transactions = trxs;

    auto batch = db->createWriteBatch();
    db->savePeriodData(period_data, batch);

    db->commitWriteBatch(batch);

    auto result = SUT->finalize(std::move(period_data), {dag_blk.getHash()}).get();
    ++expected_blk_num;
    auto const& blk_h = *result->final_chain_blk;
    EXPECT_EQ(util::rlp_enc(blk_h), util::rlp_enc(*SUT->block_header(blk_h.number)));
    EXPECT_EQ(util::rlp_enc(blk_h), util::rlp_enc(*SUT->block_header()));
    auto const& receipts = result->trx_receipts;
    EXPECT_EQ(blk_h.hash, SUT->block_header()->hash);
    EXPECT_EQ(blk_h.hash, SUT->block_hash());
    EXPECT_EQ(blk_h.parent_hash, SUT->block_header(expected_blk_num - 1)->hash);
    EXPECT_EQ(blk_h.number, expected_blk_num);
    EXPECT_EQ(blk_h.number, SUT->last_block_number());
    EXPECT_EQ(SUT->transactionCount(blk_h.number), trxs.size());
    for (size_t i = 0; i < trxs.size(); i++) EXPECT_EQ(*SUT->transactions(blk_h.number)[i], *trxs[i]);
    EXPECT_EQ(*SUT->block_number(*SUT->block_hash(blk_h.number)), expected_blk_num);
    EXPECT_EQ(blk_h.author, pbft_block->getBeneficiary());
    EXPECT_EQ(blk_h.timestamp, pbft_block->getTimestamp());
    EXPECT_EQ(receipts.size(), trxs.size());
    EXPECT_EQ(blk_h.transactions_root,
              trieRootOver(
                  trxs.size(), [&](auto i) { return dev::rlp(i); }, [&](auto i) { return trxs[i]->rlp(); }));
    EXPECT_EQ(blk_h.receipts_root, trieRootOver(
                                       trxs.size(), [&](auto i) { return dev::rlp(i); },
                                       [&](auto i) { return util::rlp_enc(receipts[i]); }));
    EXPECT_EQ(blk_h.gas_limit, FinalChain::GAS_LIMIT);
    EXPECT_EQ(blk_h.extra_data, bytes());
    EXPECT_EQ(blk_h.nonce(), Nonce());
    EXPECT_EQ(blk_h.difficulty(), 0);
    EXPECT_EQ(blk_h.mix_hash(), h256());
    EXPECT_EQ(blk_h.uncles_hash(), EmptyRLPListSHA3());
    EXPECT_TRUE(!blk_h.state_root.isZero());
    LogBloom expected_block_log_bloom;
    std::unordered_map<addr_t, u256> expected_balance_changes;
    std::unordered_set<addr_t> all_addrs_w_changed_balance;
    uint64_t cumulative_gas_used_actual = 0;
    for (size_t i = 0; i < trxs.size(); ++i) {
      auto const& trx = trxs[i];
      auto const& r = receipts[i];
      if (!opts.expect_to_fail) {
        EXPECT_TRUE(r.gas_used != 0);
      }
      EXPECT_EQ(util::rlp_enc(r), util::rlp_enc(*SUT->transaction_receipt(trx->getHash())));
      cumulative_gas_used_actual += r.gas_used;
      if (assume_only_toplevel_transfers && trx->getValue() != 0 && r.status_code == 1) {
        auto const& sender = trx->getSender();
        auto const& sender_bal = expected_balances[sender] -= trx->getValue();
        auto const& receiver = !trx->getReceiver() ? *r.new_contract_address : *trx->getReceiver();
        all_addrs_w_changed_balance.insert(sender);
        all_addrs_w_changed_balance.insert(receiver);
        auto const& receiver_bal = expected_balances[receiver] += trx->getValue();
        if (SUT->get_account(sender)->code_size == 0) {
          expected_balance_changes[sender] = sender_bal;
        }
        if (SUT->get_account(receiver)->code_size == 0) {
          expected_balance_changes[receiver] = receiver_bal;
        }
      }
      if (opts.expect_to_fail) {
        EXPECT_EQ(r.status_code, 0);
      } else if (!opts.dont_assume_all_trx_success) {
        EXPECT_EQ(r.status_code, 1);
      }

      if (!opts.dont_assume_no_logs) {
        EXPECT_EQ(r.logs.size(), 0);
        EXPECT_EQ(r.bloom(), LogBloom());
      }
      expected_block_log_bloom |= r.bloom();
      auto trx_loc = *SUT->transaction_location(trx->getHash());
      EXPECT_EQ(trx_loc.blk_n, blk_h.number);
      EXPECT_EQ(trx_loc.index, i);
    }
    EXPECT_EQ(blk_h.gas_used, cumulative_gas_used_actual);
    if (!receipts.empty()) {
      EXPECT_EQ(receipts.back().cumulative_gas_used, cumulative_gas_used_actual);
    }
    EXPECT_EQ(blk_h.log_bloom, expected_block_log_bloom);
    if (assume_only_toplevel_transfers) {
      for (auto const& addr : all_addrs_w_changed_balance) {
        EXPECT_EQ(SUT->get_account(addr)->balance, expected_balances[addr]);
      }
    }
    return result;
  }

  void fillConfigForGenesisTests(const addr_t& init_address) {
    cfg.final_chain.state.genesis_balances = {};
    cfg.final_chain.state.genesis_balances[init_address] = 1000000000 * kOneTara;
    cfg.final_chain.state.dpos.emplace();
    cfg.final_chain.state.dpos->eligibility_balance_threshold = 100000 * kOneTara;
    cfg.final_chain.state.dpos->vote_eligibility_balance_step = 10000 * kOneTara;
    cfg.final_chain.state.dpos->validator_maximum_stake = 10000000 * kOneTara;
    cfg.final_chain.state.dpos->minimum_deposit = 1000 * kOneTara;
    cfg.final_chain.state.dpos->eligibility_balance_threshold = 1000 * kOneTara;
    cfg.final_chain.state.dpos->yield_percentage = 10;
    cfg.final_chain.state.dpos->blocks_per_year = 1000;
  }

  template <class T, class U>
  static h256 trieRootOver(uint _itemCount, T const& _getKey, U const& _getValue) {
    dev::BytesMap m;
    for (uint i = 0; i < _itemCount; ++i) {
      m[_getKey(i)] = _getValue(i);
    }
    return hash256(m);
  }
};

TEST_F(FinalChainTest, genesis_balances) {
  cfg.final_chain.state.dpos = std::nullopt;
  cfg.final_chain.state.genesis_balances = {};
  cfg.final_chain.state.genesis_balances[addr_t::random()] = 0;
  cfg.final_chain.state.genesis_balances[addr_t::random()] = 1000;
  cfg.final_chain.state.genesis_balances[addr_t::random()] = 100000;
  init();
}

// TEST_F(FinalChainTest, update_state_config) {
//   init();
//   cfg.final_chain.state.hardforks.fix_genesis_fork_block = 2222222;
//   SUT->update_state_config(cfg.final_chain.state);
// }

TEST_F(FinalChainTest, contract) {
  auto sender_keys = dev::KeyPair::create();
  auto const& addr = sender_keys.address();
  auto const& sk = sender_keys.secret();
  cfg.final_chain.state.genesis_balances = {};
  cfg.final_chain.state.genesis_balances[addr] = 100000;
  cfg.final_chain.state.execution_options.disable_nonce_check = true;
  init();
  auto trx = std::make_shared<Transaction>(0, 100, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
  auto result = advance({trx});
  auto contract_addr = result->trx_receipts[0].new_contract_address;
  EXPECT_EQ(contract_addr, dev::right160(dev::sha3(dev::rlpList(addr, 0))));
  auto greet = [&] {
    auto ret = SUT->call({
        addr,
        0,
        contract_addr,
        0,
        0,
        1000000,
        // greet()
        dev::fromHex("0xcfae3217"),
    });
    return dev::toHexPrefixed(ret.code_retval);
  };
  ASSERT_EQ(greet(),
            // "Hello"
            "0x0000000000000000000000000000000000000000000000000000000000000020"
            "000000000000000000000000000000000000000000000000000000000000000548"
            "656c6c6f000000000000000000000000000000000000000000000000000000");
  {
    advance({
        std::make_shared<Transaction>(0, 11, 0, 1000000,
                                      // setGreeting("Hola")
                                      dev::fromHex("0xa4136862000000000000000000000000000000000000000000000000"
                                                   "00000000000000200000000000000000000000000000000000000000000"
                                                   "000000000000000000004486f6c61000000000000000000000000000000"
                                                   "00000000000000000000000000"),
                                      sk, contract_addr),
    });
  }
  ASSERT_EQ(greet(),
            // "Hola"
            "0x000000000000000000000000000000000000000000000000000000000000002000"
            "00000000000000000000000000000000000000000000000000000000000004486f"
            "6c6100000000000000000000000000000000000000000000000000000000");
}

TEST_F(FinalChainTest, coin_transfers) {
  constexpr size_t NUM_ACCS = 500;
  cfg.final_chain.state.genesis_balances = {};
  std::vector<dev::KeyPair> keys;
  keys.reserve(NUM_ACCS);
  for (size_t i = 0; i < NUM_ACCS; ++i) {
    auto const& k = keys.emplace_back(dev::KeyPair::create());
    cfg.final_chain.state.genesis_balances[k.address()] = std::numeric_limits<u256>::max() / NUM_ACCS;
  }
  init();
  constexpr auto TRX_GAS = 100000;
  advance({
      std::make_shared<Transaction>(0, 13, 0, TRX_GAS, dev::bytes(), keys[10].secret(), keys[10].address()),
      std::make_shared<Transaction>(0, 11300, 0, TRX_GAS, dev::bytes(), keys[102].secret(), keys[44].address()),
      std::make_shared<Transaction>(0, 1040, 0, TRX_GAS, dev::bytes(), keys[122].secret(), keys[50].address()),
  });
  advance({});
  advance({
      std::make_shared<Transaction>(0, 0, 0, TRX_GAS, dev::bytes(), keys[2].secret(), keys[1].address()),
      std::make_shared<Transaction>(0, 131, 0, TRX_GAS, dev::bytes(), keys[133].secret(), keys[133].address()),
  });
  advance({
      std::make_shared<Transaction>(0, 100441, 0, TRX_GAS, dev::bytes(), keys[177].secret(), keys[431].address()),
      std::make_shared<Transaction>(0, 2300, 0, TRX_GAS, dev::bytes(), keys[131].secret(), keys[343].address()),
      std::make_shared<Transaction>(0, 130, 0, TRX_GAS, dev::bytes(), keys[11].secret(), keys[23].address()),
  });
  advance({});
  advance({
      std::make_shared<Transaction>(0, 100431, 0, TRX_GAS, dev::bytes(), keys[135].secret(), keys[232].address()),
      std::make_shared<Transaction>(0, 13411, 0, TRX_GAS, dev::bytes(), keys[112].secret(), keys[34].address()),
      std::make_shared<Transaction>(0, 130, 0, TRX_GAS, dev::bytes(), keys[134].secret(), keys[233].address()),
      std::make_shared<Transaction>(0, 343434, 0, TRX_GAS, dev::bytes(), keys[13].secret(), keys[213].address()),
      std::make_shared<Transaction>(0, 131313, 0, TRX_GAS, dev::bytes(), keys[405].secret(), keys[344].address()),
      std::make_shared<Transaction>(0, 143430, 0, TRX_GAS, dev::bytes(), keys[331].secret(), keys[420].address()),
      std::make_shared<Transaction>(0, 1313145, 0, TRX_GAS, dev::bytes(), keys[345].secret(), keys[134].address()),
  });
}

TEST_F(FinalChainTest, nonce_skipping) {
  const dev::KeyPair key = dev::KeyPair::create();
  constexpr auto TRX_GAS = 100000;
  cfg.final_chain.state.genesis_balances = {};
  cfg.final_chain.state.genesis_balances[key.address()] = std::numeric_limits<u256>::max();
  cfg.final_chain.state.execution_options.disable_nonce_check = false;
  cfg.final_chain.state.block_rewards_options.disable_contract_distribution = true;
  init();
  advance({std::make_shared<Transaction>(2, 13, 0, TRX_GAS, dev::bytes(), key.secret(), key.address())},
          {false, false, true});

  cfg.final_chain.state.execution_options.enable_nonce_skipping = true;
  advance({std::make_shared<Transaction>(5, 13, 0, TRX_GAS, dev::bytes(), key.secret(), key.address())});
}

TEST_F(FinalChainTest, initial_validators) {
  const dev::KeyPair key = dev::KeyPair::create();
  const std::vector<dev::KeyPair> validator_keys = {dev::KeyPair::create(), dev::KeyPair::create(),
                                                    dev::KeyPair::create()};
  fillConfigForGenesisTests(key.address());

  for (const auto& vk : validator_keys) {
    const auto [vrf_key, _] = taraxa::vrf_wrapper::getVrfKeyPair();
    state_api::ValidatorInfo validator{vk.address(), key.address(), vrf_key, 0, "", "", {}};
    validator.delegations.emplace(key.address(), cfg.final_chain.state.dpos->validator_maximum_stake);
    cfg.final_chain.state.dpos->initial_validators.emplace_back(validator);
  }

  init();
  const auto votes_per_address =
      cfg.final_chain.state.dpos->validator_maximum_stake / cfg.final_chain.state.dpos->vote_eligibility_balance_step;
  const auto total_votes = SUT->dpos_eligible_total_vote_count(SUT->last_block_number());
  for (const auto& vk : validator_keys) {
    const auto address_votes = SUT->dpos_eligible_vote_count(SUT->last_block_number(), vk.address());
    EXPECT_EQ(votes_per_address, address_votes);
    EXPECT_EQ(validator_keys.size() * votes_per_address, total_votes);
  }
}

TEST_F(FinalChainTest, initial_validator_exceed_maximum_stake) {
  const dev::KeyPair key = dev::KeyPair::create();
  const dev::KeyPair validator_key = dev::KeyPair::create();
  const auto [vrf_key, _] = taraxa::vrf_wrapper::getVrfKeyPair();
  fillConfigForGenesisTests(key.address());

  state_api::ValidatorInfo validator{validator_key.address(), key.address(), vrf_key, 0, "", "", {}};
  validator.delegations.emplace(key.address(), cfg.final_chain.state.dpos->validator_maximum_stake);
  validator.delegations.emplace(validator_key.address(), cfg.final_chain.state.dpos->minimum_deposit);
  cfg.final_chain.state.dpos->initial_validators.emplace_back(validator);

  EXPECT_THROW(init(), std::exception);
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
