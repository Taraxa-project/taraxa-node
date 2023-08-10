#include "final_chain/final_chain.hpp"

#include <optional>
#include <vector>

#include "common/constants.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/config.hpp"
#include "final_chain/trie_common.hpp"
#include "libdevcore/CommonJS.h"
#include "network/rpc/eth/Eth.h"
#include "test_util/gtest.hpp"
#include "test_util/samples.hpp"
#include "test_util/test_util.hpp"
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
  FullNodeConfig cfg = FullNodeConfig();
  std::shared_ptr<FinalChain> SUT;
  bool assume_only_toplevel_transfers = true;
  std::unordered_map<addr_t, u256> expected_balances;
  uint64_t expected_blk_num = 0;
  void init() {
    SUT = NewFinalChain(db, cfg);
    const auto& effective_balances = effective_initial_balances(cfg.genesis.state);
    cfg.genesis.state.dpos.yield_percentage = 0;
    for (const auto& [addr, _] : cfg.genesis.state.initial_balances) {
      auto acc_actual = SUT->get_account(addr);
      ASSERT_TRUE(acc_actual);
      const auto expected_bal = effective_balances.at(addr);
      ASSERT_EQ(acc_actual->balance, expected_bal);
      expected_balances[addr] = expected_bal;
    }
  }

  auto advance(const SharedTransactions& trxs, advance_check_opts opts = {}) {
    std::vector<h256> trx_hashes;
    ++expected_blk_num;
    for (const auto& trx : trxs) {
      trx_hashes.emplace_back(trx->getHash());
    }

    auto proposer_keys = dev::KeyPair::create();
    DagBlock dag_blk({}, {}, {}, trx_hashes, {}, {}, proposer_keys.secret());
    db->saveDagBlock(dag_blk);
    std::vector<vote_hash_t> reward_votes_hashes;
    auto pbft_block =
        std::make_shared<PbftBlock>(kNullBlockHash, kNullBlockHash, kNullBlockHash, kNullBlockHash, expected_blk_num,
                                    addr_t::random(), proposer_keys.secret(), std::move(reward_votes_hashes));

    std::vector<std::shared_ptr<Vote>> votes;
    PeriodData period_data(pbft_block, votes);
    period_data.dag_blocks.push_back(dag_blk);
    period_data.transactions = trxs;
    if (pbft_block->getPeriod() > 1) {
      period_data.previous_block_cert_votes = {
          genDummyVote(PbftVoteTypes::cert_vote, pbft_block->getPeriod() - 1, 1, 3, pbft_block->getBlockHash())};
    }

    auto batch = db->createWriteBatch();
    db->savePeriodData(period_data, batch);
    db->commitWriteBatch(batch);

    auto result = SUT->finalize(std::move(period_data), {dag_blk.getHash()}).get();
    const auto& blk_h = *result->final_chain_blk;
    EXPECT_EQ(util::rlp_enc(blk_h), util::rlp_enc(*SUT->block_header(blk_h.number)));
    EXPECT_EQ(util::rlp_enc(blk_h), util::rlp_enc(*SUT->block_header()));
    const auto& receipts = result->trx_receipts;
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
    EXPECT_EQ(blk_h.gas_limit, cfg.genesis.pbft.gas_limit);
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
      const auto& trx = trxs[i];
      const auto& r = receipts[i];
      if (!opts.expect_to_fail) {
        EXPECT_TRUE(r.gas_used != 0);
      }
      EXPECT_EQ(util::rlp_enc(r), util::rlp_enc(*SUT->transaction_receipt(trx->getHash())));
      cumulative_gas_used_actual += r.gas_used;
      if (assume_only_toplevel_transfers && trx->getValue() != 0 && r.status_code == 1) {
        const auto& sender = trx->getSender();
        expected_balances[sender] -= trx->getValue();
        expected_balances[sender] -= r.gas_used * trx->getGasPrice();
        const auto& receiver = !trx->getReceiver() ? *r.new_contract_address : *trx->getReceiver();
        all_addrs_w_changed_balance.insert(sender);
        all_addrs_w_changed_balance.insert(receiver);
        expected_balances[receiver] += trx->getValue();
        if (SUT->get_account(sender)->code_size == 0) {
          expected_balance_changes[sender] = expected_balances[sender];
        }
        if (SUT->get_account(receiver)->code_size == 0) {
          expected_balance_changes[receiver] = expected_balances[receiver];
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
      for (const auto& addr : all_addrs_w_changed_balance) {
        EXPECT_EQ(SUT->get_account(addr)->balance, expected_balances[addr]);
      }
    }
    return result;
  }

  void fillConfigForGenesisTests(const addr_t& init_address) {
    cfg.genesis.state.initial_balances = {};
    cfg.genesis.state.initial_balances[init_address] = 1000000000 * kOneTara;
    cfg.genesis.state.dpos.eligibility_balance_threshold = 100000 * kOneTara;
    cfg.genesis.state.dpos.vote_eligibility_balance_step = 10000 * kOneTara;
    cfg.genesis.state.dpos.validator_maximum_stake = 10000000 * kOneTara;
    cfg.genesis.state.dpos.minimum_deposit = 1000 * kOneTara;
    cfg.genesis.state.dpos.eligibility_balance_threshold = 1000 * kOneTara;
    cfg.genesis.state.dpos.yield_percentage = 10;
    cfg.genesis.state.dpos.blocks_per_year = 1000;
  }

  template <class T, class U>
  static h256 trieRootOver(uint _itemCount, const T& _getKey, const U& _getValue) {
    dev::BytesMap m;
    for (uint i = 0; i < _itemCount; ++i) {
      m[_getKey(i)] = _getValue(i);
    }
    return hash256(m);
  }
};

TEST_F(FinalChainTest, initial_balances) {
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr_t::random()] = 1;
  cfg.genesis.state.initial_balances[addr_t::random()] = 1000;
  cfg.genesis.state.initial_balances[addr_t::random()] = 100000;
  init();
}

TEST_F(FinalChainTest, contract) {
  auto sender_keys = dev::KeyPair::create();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();
  auto nonce = 0;
  auto trx = std::make_shared<Transaction>(nonce++, 0, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
  auto result = advance({trx});
  auto contract_addr = result->trx_receipts[0].new_contract_address;
  std::cout << "contract_addr " << contract_addr->toString() << std::endl;
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
        std::make_shared<Transaction>(nonce++, 11, 0, 1000000,
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
  constexpr size_t NUM_ACCS = 5;
  cfg.genesis.state.initial_balances = {};
  std::vector<dev::KeyPair> keys;
  keys.reserve(NUM_ACCS);
  for (size_t i = 0; i < NUM_ACCS; ++i) {
    const auto& k = keys.emplace_back(dev::KeyPair::create());
    cfg.genesis.state.initial_balances[k.address()] = std::numeric_limits<u256>::max() / NUM_ACCS;
  }

  init();
  advance({});

  constexpr auto TRX_GAS = 100000;
  advance({
      std::make_shared<Transaction>(1, 13, 0, TRX_GAS, dev::bytes(), keys[0].secret(), keys[1].address()),
      std::make_shared<Transaction>(1, 11300, 0, TRX_GAS, dev::bytes(), keys[1].secret(), keys[1].address()),
      std::make_shared<Transaction>(1, 1040, 0, TRX_GAS, dev::bytes(), keys[2].secret(), keys[1].address()),
  });
  advance({});
  advance({
      std::make_shared<Transaction>(1, 0, 0, TRX_GAS, dev::bytes(), keys[3].secret(), keys[1].address()),
      std::make_shared<Transaction>(1, 131, 0, TRX_GAS, dev::bytes(), keys[4].secret(), keys[1].address()),
  });
  advance({
      std::make_shared<Transaction>(2, 100441, 0, TRX_GAS, dev::bytes(), keys[0].secret(), keys[1].address()),
      std::make_shared<Transaction>(2, 2300, 0, TRX_GAS, dev::bytes(), keys[1].secret(), keys[1].address()),
      std::make_shared<Transaction>(2, 130, 0, TRX_GAS, dev::bytes(), keys[2].secret(), keys[1].address()),
  });
  advance({});
  advance({
      std::make_shared<Transaction>(2, 100431, 0, TRX_GAS, dev::bytes(), keys[3].secret(), keys[1].address()),
      std::make_shared<Transaction>(2, 13411, 0, TRX_GAS, dev::bytes(), keys[4].secret(), keys[1].address()),
      std::make_shared<Transaction>(3, 130, 0, TRX_GAS, dev::bytes(), keys[0].secret(), keys[1].address()),
      std::make_shared<Transaction>(3, 343434, 0, TRX_GAS, dev::bytes(), keys[1].secret(), keys[1].address()),
      std::make_shared<Transaction>(3, 131313, 0, TRX_GAS, dev::bytes(), keys[2].secret(), keys[1].address()),
      std::make_shared<Transaction>(3, 143430, 0, TRX_GAS, dev::bytes(), keys[3].secret(), keys[1].address()),
      std::make_shared<Transaction>(3, 1313145, 0, TRX_GAS, dev::bytes(), keys[4].secret(), keys[1].address()),
  });
}

TEST_F(FinalChainTest, initial_validators) {
  const dev::KeyPair key = dev::KeyPair::create();
  const std::vector<dev::KeyPair> validator_keys = {dev::KeyPair::create(), dev::KeyPair::create(),
                                                    dev::KeyPair::create()};
  fillConfigForGenesisTests(key.address());

  for (const auto& vk : validator_keys) {
    const auto [vrf_key, _] = taraxa::vrf_wrapper::getVrfKeyPair();
    state_api::ValidatorInfo validator{vk.address(), key.address(), vrf_key, 0, "", "", {}};
    validator.delegations.emplace(key.address(), cfg.genesis.state.dpos.validator_maximum_stake);
    cfg.genesis.state.dpos.initial_validators.emplace_back(validator);
  }

  init();
  const auto votes_per_address =
      cfg.genesis.state.dpos.validator_maximum_stake / cfg.genesis.state.dpos.vote_eligibility_balance_step;
  const auto total_votes = SUT->dpos_eligible_total_vote_count(SUT->last_block_number());
  for (const auto& vk : validator_keys) {
    const auto address_votes = SUT->dpos_eligible_vote_count(SUT->last_block_number(), vk.address());
    EXPECT_EQ(votes_per_address, address_votes);
    EXPECT_EQ(validator_keys.size() * votes_per_address, total_votes);
  }
}

TEST_F(FinalChainTest, nonce_test) {
  auto sender_keys = dev::KeyPair::create();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  const auto receiver_addr = dev::KeyPair::create().address();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();

  auto trx1 = std::make_shared<Transaction>(0, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx2 = std::make_shared<Transaction>(1, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx3 = std::make_shared<Transaction>(2, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx4 = std::make_shared<Transaction>(3, 100, 0, 100000, dev::bytes(), sk, receiver_addr);

  advance({trx1});
  advance({trx2});
  advance({trx3});
  advance({trx4});

  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 4);

  // nonce_skipping is enabled, ok
  auto trx6 = std::make_shared<Transaction>(6, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  advance({trx6});

  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 7);

  // nonce is lower, fail
  auto trx5 = std::make_shared<Transaction>(5, 101, 0, 100000, dev::bytes(), sk, receiver_addr);
  advance({trx5}, {false, false, true});
}

TEST_F(FinalChainTest, nonce_skipping) {
  auto sender_keys = dev::KeyPair::create();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  const auto receiver_addr = dev::KeyPair::create().address();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();

  auto trx1 = std::make_shared<Transaction>(0, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx2 = std::make_shared<Transaction>(1, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx3 = std::make_shared<Transaction>(2, 100, 0, 100000, dev::bytes(), sk, receiver_addr);
  auto trx4 = std::make_shared<Transaction>(3, 100, 0, 100000, dev::bytes(), sk, receiver_addr);

  advance({trx1});
  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 1);

  advance({trx3});
  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 3);

  // fail transaction with the same nonce
  advance({trx3}, {false, false, true});

  // fail transaction with lower nonce
  advance({trx2}, {false, false, true});

  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 3);

  advance({trx4});
  ASSERT_EQ(SUT->get_account(addr)->nonce.convert_to<uint64_t>(), 4);
}

TEST_F(FinalChainTest, exec_trx_with_nonce_from_api) {
  auto sender_keys = dev::KeyPair::create();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();

  // exec trx with nonce 5 to skip some
  auto nonce = 5;
  {
    auto trx = std::make_shared<Transaction>(nonce, 0, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
    auto result = advance({trx});
  }
  // fail second trx with same nonce
  {
    auto trx = std::make_shared<Transaction>(nonce, 1, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
    auto result = advance({trx}, {false, false, true});
  }
  auto account = SUT->get_account(addr);
  ASSERT_EQ(account->nonce, nonce + 1);
  auto trx =
      std::make_shared<Transaction>(account->nonce, 1, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
  auto result = advance({trx});
}

TEST_F(FinalChainTest, new_contract_address) {
  auto new_contract_address = [](u256 nonce, const addr_t& sender) {
    return dev::right160(dev::sha3(dev::rlpList(sender, nonce)));
  };
  {
    const auto& sender = addr_t("0xbc3f916f3384eb088b2c662f59aca594a5b25b02");
    // https://rinkeby.etherscan.io/tx/0x2c4d922f7031584ade06b04aa661c6d045482450c36c1c844848adafca29c026
    // from: 0xbc3f916f3384eb088b2c662f59aca594a5b25b02 nonce:58 created: 0x313312e14cbdad86d616debd37e0ecf0b3dfef03
    // https://rinkeby.etherscan.io/tx/0x0923ba99c0839b9ef761e1e61b7b7f20eb9d3fd48a955ae34e591c9e27e0dcce
    // from: 0xbc3f916f3384eb088b2c662f59aca594a5b25b02 nonce:59 created: 0x22f95efe25ff7dce8ed5066acff5572f9f1683e8
    // https://rinkeby.etherscan.io/tx/0x555b5fa200f768da0df1c7141321b76251c45d63aeba8d2c840fa046128d92a6
    // from: 0xbc3f916f3384eb088b2c662f59aca594a5b25b02 nonce:60 created: 0x2109b75cca2094f5df48a7a2f8a4514b521038bc
    std::map<uint8_t, addr_t> nonce_and_address = {
        {58, addr_t("0x313312e14cbdad86d616debd37e0ecf0b3dfef03")},
        {59, addr_t("0x22f95efe25ff7dce8ed5066acff5572f9f1683e8")},
        {60, addr_t("0x2109b75cca2094f5df48a7a2f8a4514b521038bc")},
    };

    for (const auto& p : nonce_and_address) {
      ASSERT_EQ(new_contract_address(p.first, sender), p.second);
    }
  }

  auto sender_keys = dev::KeyPair::create();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();

  auto nonce = 0;
  {
    auto trx = std::make_shared<Transaction>(nonce++, 0, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
    auto result = advance({trx});
    auto contract_addr = result->trx_receipts[0].new_contract_address;
    ASSERT_EQ(contract_addr, new_contract_address(trx->getNonce(), addr));
  }

  // skip few transactions, but new contract address should be still correct
  {
    nonce = 5;
    auto trx = std::make_shared<Transaction>(nonce++, 0, 0, 1000000, dev::fromHex(samples::greeter_contract_code), sk);
    auto result = advance({trx});
    auto contract_addr = result->trx_receipts[0].new_contract_address;
    ASSERT_EQ(contract_addr, new_contract_address(trx->getNonce(), addr));
  }
}

TEST_F(FinalChainTest, failed_transaction_fee) {
  auto sender_keys = dev::KeyPair::create();
  auto gas = 30000;

  const auto& receiver = dev::KeyPair::create().address();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();
  auto trx1 = std::make_shared<Transaction>(1, 100, 1, gas, dev::bytes(), sk, receiver);
  auto trx2 = std::make_shared<Transaction>(2, 100, 1, gas, dev::bytes(), sk, receiver);
  auto trx3 = std::make_shared<Transaction>(3, 100, 1, gas, dev::bytes(), sk, receiver);
  auto trx2_1 = std::make_shared<Transaction>(2, 101, 1, gas, dev::bytes(), sk, receiver);

  advance({trx1});
  auto blk = SUT->block_header(expected_blk_num);
  auto proposer_balance = SUT->getBalance(blk->author);
  EXPECT_EQ(proposer_balance.first, 21000);
  advance({trx2});
  advance({trx3});

  {
    // low nonce trx should fail and consume all gas
    auto balance_before = SUT->get_account(addr)->balance;
    advance({trx2_1}, {false, false, true});
    auto receipt = SUT->transaction_receipt(trx2_1->getHash());
    EXPECT_EQ(receipt->gas_used, gas);
    EXPECT_EQ(balance_before - SUT->get_account(addr)->balance, receipt->gas_used * trx2_1->getGasPrice());
  }
  {
    // transaction gas is bigger then current account balance. Use closest int as gas used and decrease sender balance
    // by gas_used * gas_price
    ASSERT_GE(gas, SUT->get_account(addr)->balance);
    auto balance_before = SUT->get_account(addr)->balance;
    auto gas_price = 3;
    auto trx4 = std::make_shared<Transaction>(4, 100, gas_price, gas, dev::bytes(), sk, receiver);
    advance({trx4}, {false, false, true});
    auto receipt = SUT->transaction_receipt(trx4->getHash());
    EXPECT_GT(balance_before % gas_price, 0);
    EXPECT_EQ(receipt->gas_used, balance_before / gas_price);
    EXPECT_EQ(SUT->get_account(addr)->balance, balance_before % gas_price);
  }
}

TEST_F(FinalChainTest, revert_reason) {
  // contract TestRevert {
  //   function test(bool arg) public pure {
  //       require(arg, "arg required");
  //   }
  // }
  const auto test_contract_code =
      "608060405234801561001057600080fd5b506101ac806100206000396000f3fe608060405234801561001057600080fd5b50600436106100"
      "2b5760003560e01c806336091dff14610030575b600080fd5b61004a600480360381019061004591906100cc565b61004c565b005b806100"
      "8c576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161008390610156565b60405180"
      "910390fd5b50565b600080fd5b60008115159050919050565b6100a981610094565b81146100b457600080fd5b50565b6000813590506100"
      "c6816100a0565b92915050565b6000602082840312156100e2576100e161008f565b5b60006100f0848285016100b7565b91505092915050"
      "565b600082825260208201905092915050565b7f617267207265717569726564000000000000000000000000000000000000000060008201"
      "5250565b6000610140600c836100f9565b915061014b8261010a565b602082019050919050565b6000602082019050818103600083015261"
      "016f81610133565b905091905056fea2646970667358221220846c5a92aab30dade0d92661a25b1fd6ba9a914fd114f2f264c2003b5abdda"
      "db64736f6c63430008120033";
  auto sender_keys = dev::KeyPair::create();
  const auto& from = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[from] = u256("10000000000000000000000");
  init();

  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = cfg.genesis.chain_id;
  eth_rpc_params.gas_limit = cfg.genesis.dag.gas_limit;
  eth_rpc_params.final_chain = SUT;
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  auto nonce = 0;
  auto trx1 = std::make_shared<Transaction>(nonce++, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(test_contract_code), sk);
  auto result = advance({trx1});
  auto test_contract_addr = result->trx_receipts[0].new_contract_address;
  EXPECT_EQ(test_contract_addr, dev::right160(dev::sha3(dev::rlpList(from, 0))));
  auto call_data = "0x36091dff0000000000000000000000000000000000000000000000000000000000000000";
  {
    Json::Value est(Json::objectValue);
    est["to"] = dev::toHex(*test_contract_addr);
    est["from"] = dev::toHex(from);
    est["data"] = call_data;
    EXPECT_THROW_WITH(dev::jsToInt(eth_json_rpc->eth_estimateGas(est)), std::exception,
                      "evm: execution reverted: arg required");
    EXPECT_THROW_WITH(eth_json_rpc->eth_call(est, "latest"), std::exception, "evm: execution reverted: arg required");

    auto gas = 100000;
    auto trx = std::make_shared<Transaction>(2, 0, 1, gas, dev::fromHex(call_data), sk, test_contract_addr);
    auto result = advance({trx}, {0, 0, 1});
    auto receipt = result->trx_receipts.front();
    ASSERT_EQ(receipt.status_code, 0);  // failed
    ASSERT_GT(gas, receipt.gas_used);   // we aren't spending all gas in such cases
  }
}

TEST_F(FinalChainTest, incorrect_estimation_regress) {
  // contract Receiver {
  //     uint256 public receivedETH;
  //     receive() external payable {
  //         receivedETH += msg.value;
  //     }
  // }
  const auto receiver_contract_code =
      "608060405234801561001057600080fd5b5061012d806100206000396000f3fe608060405260043610601f5760003560e01c8063820bec9d"
      "14603f57603a565b36603a57346000808282546032919060a4565b925050819055005b600080fd5b348015604a57600080fd5b5060516065"
      "565b604051605c919060de565b60405180910390f35b60005481565b6000819050919050565b7f4e487b7100000000000000000000000000"
      "000000000000000000000000000000600052601160045260246000fd5b600060ad82606b565b915060b683606b565b925082820190508082"
      "111560cb5760ca6075565b5b92915050565b60d881606b565b82525050565b600060208201905060f1600083018460d1565b9291505056fe"
      "a264697066735822122099ea1faf8b41cec96834060f2daaea3ae5c03561e110bdcf5a74ce041ddb497164736f6c63430008120033";

  // contract SendFunction {
  //     function send(address to) external payable {
  //         (bool success,) = to.call{value: msg.value}("");
  //         if (!success) {
  //             revert("Failed to send ETH");
  //         }
  //     }
  // }
  const auto sender_contract_code =
      "608060405234801561001057600080fd5b50610278806100206000396000f3fe60806040526004361061001e5760003560e01c80633e58c5"
      "8c14610023575b600080fd5b61003d60048036038101906100389190610152565b61003f565b005b60008173ffffffffffffffffffffffff"
      "ffffffffffffffff1634604051610065906101b0565b60006040518083038185875af1925050503d80600081146100a2576040519150601f"
      "19603f3d011682016040523d82523d6000602084013e6100a7565b606091505b50509050806100eb576040517f08c379a000000000000000"
      "00000000000000000000000000000000000000000081526004016100e290610222565b60405180910390fd5b5050565b600080fd5b600073"
      "ffffffffffffffffffffffffffffffffffffffff82169050919050565b600061011f826100f4565b9050919050565b61012f81610114565b"
      "811461013a57600080fd5b50565b60008135905061014c81610126565b92915050565b600060208284031215610168576101676100ef565b"
      "5b60006101768482850161013d565b91505092915050565b600081905092915050565b50565b600061019a60008361017f565b91506101a5"
      "8261018a565b600082019050919050565b60006101bb8261018d565b9150819050919050565b600082825260208201905092915050565b7f"
      "4661696c656420746f2073656e64204554480000000000000000000000000000600082015250565b600061020c6012836101c5565b915061"
      "0217826101d6565b602082019050919050565b6000602082019050818103600083015261023b816101ff565b905091905056fea264697066"
      "73582212205fd48a05d31cae1309b1a3bb8fe678c4bfee4cd28079acd90056ad228e18d82864736f6c63430008120033";

  auto sender_keys = dev::KeyPair::create();
  const auto& from = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[from] = u256("10000000000000000000000");
  // disable balances check as we have internal transfer
  assume_only_toplevel_transfers = false;
  init();

  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = cfg.genesis.chain_id;
  eth_rpc_params.gas_limit = cfg.genesis.dag.gas_limit;
  eth_rpc_params.final_chain = SUT;
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  auto nonce = 0;
  auto trx1 = std::make_shared<Transaction>(nonce++, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(receiver_contract_code), sk);
  auto trx2 = std::make_shared<Transaction>(nonce++, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(sender_contract_code), sk);
  auto result = advance({trx1, trx2});
  auto receiver_contract_addr = result->trx_receipts[0].new_contract_address;
  auto sender_contract_addr = result->trx_receipts[1].new_contract_address;
  EXPECT_EQ(receiver_contract_addr, dev::right160(dev::sha3(dev::rlpList(from, 0))));

  const auto call_data = "0x3e58c58c000000000000000000000000" + receiver_contract_addr->toString();
  const auto value = 10000;
  {
    Json::Value est(Json::objectValue);
    est["to"] = dev::toHex(*sender_contract_addr);
    est["from"] = dev::toHex(from);
    est["value"] = value;
    est["data"] = call_data;
    auto estimate = dev::jsToInt(eth_json_rpc->eth_estimateGas(est));
    est["gas"] = dev::toJS(estimate);
    eth_json_rpc->eth_call(est, "latest");
  }
}

TEST_F(FinalChainTest, get_logs_multiple_topics) {
  // contract Events {
  //     event Event1(uint256 indexed v1);
  //     event Event2(uint256 indexed v1,uint256 indexed v2);
  //     event Event3(uint256 indexed v1,uint256 indexed v2,uint256 indexed v3);
  //     function method1(uint256 v1) public {
  //         emit Event1(v1);
  //     }
  //     function method2(uint256 v1, uint256 v2) public {
  //         emit Event2(v1, v2);
  //     }
  //     function method3(uint256 v1, uint256 v2, uint256 v3) public {
  //         emit Event3(v1, v2, v3);
  //     }
  // }
  const auto events_contract_code =
      "608060405234801561001057600080fd5b50610261806100206000396000f3fe608060405234801561001057600080fd5b50600436106100"
      "415760003560e01c8063110d99ed14610046578063d6f7f2a114610062578063ffcd960e1461007e575b600080fd5b610060600480360381"
      "019061005b919061016b565b61009a565b005b61007c60048036038101906100779190610198565b6100ca565b005b610098600480360381"
      "019061009391906101d8565b6100fc565b005b807f04474795f5b996ff80cb47c148d4c5ccdbe09ef27551820caa9c2f8ed149cce3604051"
      "60405180910390a250565b80827f6a822560072e19c1981d3d3bb11e5954a77efa0caf306eb08d053f37de0040ba60405160405180910390"
      "a35050565b8082847fac279a174af532aabe2bdfe61037bff7cfa74374d4d24034e97609940e4e2ac960405160405180910390a450505056"
      "5b600080fd5b6000819050919050565b61014881610135565b811461015357600080fd5b50565b6000813590506101658161013f565b9291"
      "5050565b60006020828403121561018157610180610130565b5b600061018f84828501610156565b91505092915050565b60008060408385"
      "0312156101af576101ae610130565b5b60006101bd85828601610156565b92505060206101ce85828601610156565b915050925092905056"
      "5b6000806000606084860312156101f1576101f0610130565b5b60006101ff86828701610156565b93505060206102108682870161015656"
      "5b925050604061022186828701610156565b915050925092509256fea264697066735822122005a8bf7a7bc842378d30f7446847533e0b35"
      "074e5453f29fe8762c0eb4d6f4ba64736f6c63430008120033";

  auto sender_keys = dev::KeyPair::create();
  const auto& from = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[from] = u256("10000000000000000000000");
  init();

  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = cfg.genesis.chain_id;
  eth_rpc_params.gas_limit = cfg.genesis.dag.gas_limit;
  eth_rpc_params.final_chain = SUT;
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  auto nonce = 0;

  auto trx1 = std::make_shared<Transaction>(nonce++, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(events_contract_code), sk);
  auto result = advance({trx1});
  auto contract_addr = result->trx_receipts[0].new_contract_address;

  auto to_call_param = [](uint64_t v) -> std::string {
    auto str = std::to_string(v);
    return std::string(64 - str.size(), '0') + str;
  };
  auto make_call_trx = [&](const std::string& method, const std::vector<uint64_t>& params) {
    auto params_str = std::accumulate(params.begin(), params.end(), std::string(),
                                      [&](const std::string& r, uint64_t p) { return r + to_call_param(p); });
    return std::make_shared<Transaction>(nonce++, 0, 0, TEST_TX_GAS_LIMIT, dev::fromHex(method + params_str), sk,
                                         contract_addr);
  };
  auto method1 = "0x110d99ed";
  auto method2 = "0xd6f7f2a1";
  auto method3 = "0xffcd960e";
  auto topic1 = "0x04474795f5b996ff80cb47c148d4c5ccdbe09ef27551820caa9c2f8ed149cce3";
  auto topic2 = "0x6a822560072e19c1981d3d3bb11e5954a77efa0caf306eb08d053f37de0040ba";

  auto from_block = expected_blk_num;
  {
    auto trx = make_call_trx(method1, {1});
    advance({trx}, {true});
  }
  {
    auto trx = make_call_trx(method1, {2});
    advance({trx}, {true});
  }
  {
    auto trx = make_call_trx(method2, {1, 2});
    advance({trx}, {true});
  }
  {
    auto trx = make_call_trx(method3, {1, 2, 3});
    advance({trx}, {true});
  }
  {
    Json::Value topics{Json::arrayValue};
    topics.append(topic1);
    topics.append(topic2);
    Json::Value logs_obj(Json::objectValue);
    logs_obj["fromBlock"] = dev::toJS(from_block);
    logs_obj["address"] = contract_addr->toString();
    logs_obj["topics"] = Json::Value(Json::arrayValue);
    logs_obj["topics"].append(topics);
    auto res = eth_json_rpc->eth_getLogs(logs_obj);
    ASSERT_EQ(res.size(), 3);
  }
}

TEST_F(FinalChainTest, topics_size_limit) {
  init();

  net::rpc::eth::EthParams eth_rpc_params;
  eth_rpc_params.chain_id = cfg.genesis.chain_id;
  eth_rpc_params.gas_limit = cfg.genesis.dag.gas_limit;
  eth_rpc_params.final_chain = SUT;
  auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));

  Json::Value logs_obj(Json::objectValue);
  logs_obj["topics"] = Json::Value(Json::arrayValue);
  logs_obj["topics"].append("1");
  logs_obj["topics"].append("2");
  logs_obj["topics"].append("3");
  logs_obj["topics"].append("4");
  eth_json_rpc->eth_getLogs(logs_obj);
  logs_obj["topics"].append("5");
  logs_obj["topics"].append("6");
  EXPECT_THROW(eth_json_rpc->eth_getLogs(logs_obj), jsonrpc::JsonRpcException);
}

TEST_F(FinalChainTest, fee_rewards_distribution) {
  auto sender_keys = dev::KeyPair::create();
  auto gas = 30000;

  const auto& receiver = dev::KeyPair::create().address();
  const auto& addr = sender_keys.address();
  const auto& sk = sender_keys.secret();
  cfg.genesis.state.initial_balances = {};
  cfg.genesis.state.initial_balances[addr] = 100000;
  init();
  const auto gas_price = 1;
  auto trx1 = std::make_shared<Transaction>(1, 100, gas_price, gas, dev::bytes(), sk, receiver);

  auto res = advance({trx1});
  auto gas_used = res->trx_receipts.front().gas_used;
  auto blk = SUT->block_header(expected_blk_num);
  auto proposer_balance = SUT->getBalance(blk->author);
  EXPECT_EQ(proposer_balance.first, gas_used * gas_price);
}

// This test should be last as state_api isn't destructed correctly because of exception
TEST_F(FinalChainTest, initial_validator_exceed_maximum_stake) {
  const dev::KeyPair key = dev::KeyPair::create();
  const dev::KeyPair validator_key = dev::KeyPair::create();
  const auto [vrf_key, _] = taraxa::vrf_wrapper::getVrfKeyPair();
  fillConfigForGenesisTests(key.address());

  state_api::ValidatorInfo validator{validator_key.address(), key.address(), vrf_key, 0, "", "", {}};
  validator.delegations.emplace(key.address(), cfg.genesis.state.dpos.validator_maximum_stake);
  validator.delegations.emplace(validator_key.address(), cfg.genesis.state.dpos.minimum_deposit);
  cfg.genesis.state.dpos.initial_validators.emplace_back(validator);

  EXPECT_THROW(init(), std::exception);
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
