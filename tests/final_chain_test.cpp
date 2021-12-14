#include "final_chain/final_chain.hpp"

#include <chrono>
#include <optional>
#include <vector>

#include "common/constants.hpp"
#include "config/chain_config.hpp"
#include "final_chain/contract_interface.hpp"
#include "final_chain/trie_common.hpp"
#include "util_test/gtest.hpp"
#include "vote/vote.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace std::chrono;

struct advance_check_opts {
  bool dont_assume_no_logs = 0;
  bool dont_assume_all_trx_success = 0;
};

struct FinalChainTest : WithDataDir {
  shared_ptr<DbStorage> db{new DbStorage(data_dir / "db")};
  Config cfg = ChainConfig::predefined().final_chain;
  shared_ptr<FinalChain> SUT;
  bool assume_only_toplevel_transfers = true;
  unordered_map<addr_t, u256> expected_balances;
  uint64_t expected_blk_num = 0;

  void init() {
    SUT = NewFinalChain(db, cfg);
    for (auto const& [addr, _] : cfg.state.genesis_balances) {
      auto acc_actual = SUT->get_account(addr);
      ASSERT_TRUE(acc_actual);
      auto expected_bal = cfg.state.effective_genesis_balance(addr);
      ASSERT_EQ(acc_actual->balance, expected_bal);
      expected_balances[addr] = expected_bal;
    }
  }

  auto advance(Transactions const& trxs, advance_check_opts opts = {}) {
    SUT = nullptr;
    SUT = NewFinalChain(db, cfg);
    vector<h256> trx_hashes;
    int pos = 0;
    for (auto const& trx : trxs) {
      db->saveTransactionPeriod(trx.getHash(), 1, pos++);
      trx_hashes.emplace_back(trx.getHash());
    }
    DagBlock dag_blk({}, {}, {}, trx_hashes, {}, secret_t::random());
    db->saveDagBlock(dag_blk);
    PbftBlock pbft_block(blk_hash_t(), blk_hash_t(), blk_hash_t(), 1, addr_t(1), dev::KeyPair::create().secret());
    std::vector<std::shared_ptr<Vote>> votes;
    SyncBlock sync_block(std::make_shared<PbftBlock>(std::move(pbft_block)), votes);
    sync_block.dag_blocks.push_back(dag_blk);
    sync_block.transactions = trxs;

    auto batch = db->createWriteBatch();
    db->savePeriodData(sync_block, batch);

    db->commitWriteBatch(batch);
    NewBlock new_blk{
        addr_t::random(),
        uint64_t(chrono::high_resolution_clock::now().time_since_epoch().count()),
        {dag_blk.getHash()},
        h256::random(),
    };
    auto result = SUT->finalize(new_blk, 1).get();
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
    EXPECT_EQ(SUT->transactions(blk_h.number), trxs);
    EXPECT_EQ(*SUT->block_number(*SUT->block_hash(blk_h.number)), expected_blk_num);
    EXPECT_EQ(blk_h.author, new_blk.author);
    EXPECT_EQ(blk_h.timestamp, new_blk.timestamp);
    EXPECT_EQ(receipts.size(), trxs.size());
    EXPECT_EQ(blk_h.transactions_root,
              trieRootOver(
                  trxs.size(), [&](auto i) { return dev::rlp(i); }, [&](auto i) { return *trxs[i].rlp(); }));
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
    unordered_map<addr_t, u256> expected_balance_changes;
    unordered_set<addr_t> all_addrs_w_changed_balance;
    uint64_t cumulative_gas_used_actual = 0;
    for (size_t i = 0; i < trxs.size(); ++i) {
      auto const& trx = trxs[i];
      auto const& r = receipts[i];
      EXPECT_TRUE(r.gas_used != 0);
      EXPECT_EQ(util::rlp_enc(r), util::rlp_enc(*SUT->transaction_receipt(trx.getHash())));
      cumulative_gas_used_actual += r.gas_used;
      if (assume_only_toplevel_transfers && trx.getValue() != 0 && r.status_code == 1) {
        auto const& sender = trx.getSender();
        auto const& sender_bal = expected_balances[sender] -= trx.getValue();
        auto const& receiver = !trx.getReceiver() ? *r.new_contract_address : *trx.getReceiver();
        all_addrs_w_changed_balance.insert(sender);
        all_addrs_w_changed_balance.insert(receiver);
        auto const& receiver_bal = expected_balances[receiver] += trx.getValue();
        if (SUT->get_account(sender)->code_size == 0) {
          expected_balance_changes[sender] = sender_bal;
        }
        if (SUT->get_account(receiver)->code_size == 0) {
          expected_balance_changes[receiver] = receiver_bal;
        }
      }
      if (!opts.dont_assume_all_trx_success) {
        EXPECT_EQ(r.status_code, 1);
      }
      if (!opts.dont_assume_no_logs) {
        EXPECT_EQ(r.logs.size(), 0);
        EXPECT_EQ(r.bloom(), LogBloom());
      }
      expected_block_log_bloom |= r.bloom();
      auto trx_loc = *SUT->transaction_location(trx.getHash());
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

  template <class T, class U>
  static h256 trieRootOver(uint _itemCount, T const& _getKey, U const& _getValue) {
    dev::BytesMap m;
    for (uint i = 0; i < _itemCount; ++i) {
      m[_getKey(i)] = _getValue(i);
    }
    return hash256(m);
  }
};

// TEST_F(FinalChainTest, genesis_balances) {
//   cfg.state.dpos = nullopt;
//   cfg.state.genesis_balances = {};
//   cfg.state.genesis_balances[addr_t::random()] = 0;
//   cfg.state.genesis_balances[addr_t::random()] = 1000;
//   cfg.state.genesis_balances[addr_t::random()] = 100000;
//   init();
// }

TEST_F(FinalChainTest, contract) {
  std::vector<addr_t> accounts;
  for (int i = 0; i < 100; i++) {
    accounts.push_back(dev::FixedHash<20>::random());
  }
  auto sender_keys = dev::KeyPair::create();
  auto const& addr = sender_keys.address();
  auto const& sk = sender_keys.secret();
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr] = 100000;
  for (const auto& acc : accounts) {
    cfg.state.genesis_balances[acc] = 100000;
  }
  // cfg.state.dpos = nullopt;
  auto& dpos = cfg.state.dpos.emplace();
  dpos.eligibility_balance_threshold = 10;
  dpos.genesis_state[addr][addr] = dpos.eligibility_balance_threshold;
  for (const auto& acc : accounts) {
    dpos.genesis_state[acc][acc] = dpos.eligibility_balance_threshold;
  }
  init();
  static string const contract_deploy_code =
      // pragma solidity ^0.6.8;
      // contract TEST {
      //  constructor() public payable {}
      //   mapping (address => uint256) private _delegations;

      //   function set(address delegationAccount, uint256 amount) public payable {
      //     _delegations[delegationAccount] = amount;
      //   }

      //   function get(address delegationAccount) view public returns (uint256) {
      //       return _delegations[delegationAccount];
      //   }
      // }
      "6080604052610104806100136000396000f3fe60806040526004361060265760003560e01c80633825d82814602b578063c2bc2efc146056"
      "575b600080fd5b605460048036036040811015603f57600080fd5b506001600160a01b0381351690602001356097565b005b348015606157"
      "600080fd5b50608560048036036020811015607657600080fd5b50356001600160a01b031660b3565b604080519182525190819003602001"
      "90f35b6001600160a01b03909116600090815260208190526040902055565b6001600160a01b031660009081526020819052604090205490"
      "56fea26469706673582212201aab427094e4b0b9f50d1804bc02c95ffbe6a65a88e605ea6b2a36a96647a62e64736f6c63430006080033";
  Transaction trx(0, 200, 0, 0, dev::fromHex(contract_deploy_code), sk);
  auto result = advance({trx});
  auto contract_addr = result->trx_receipts[0].new_contract_address;
  EXPECT_EQ(contract_addr, dev::right160(dev::sha3(dev::rlpList(addr, 0))));

  advance({
      Transaction(1, 200, 0, 0, ContractInterface::pack("set(address,uint256)", addr, 1), sk, contract_addr),
  });

  for (const auto& acc : accounts) {
    advance({
        Transaction(1, 200, 0, 0, ContractInterface::pack("set(address,uint256)", acc, 1), sk, contract_addr),
    });
  }

  auto get = [&](const auto& acc) {
    auto ret = SUT->call({
        addr,
        0,
        contract_addr,
        0,
        0,
        0,
        ContractInterface::pack("get(address)", acc),
    });
    return dev::toHexPrefixed(ret.code_retval);
  };

  auto start = high_resolution_clock::now();
  for (const auto& acc : accounts) {
    get(acc);
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<microseconds>(stop - start);
  cout << duration.count() << endl;

  start = high_resolution_clock::now();
  for (const auto& acc : accounts) {
    SUT->dpos_eligible_vote_count(1, acc);
  }
  stop = high_resolution_clock::now();
  duration = duration_cast<microseconds>(stop - start);
  cout << duration.count() << endl;
}

// TEST_F(FinalChainTest, coin_transfers) {
//   constexpr size_t NUM_ACCS = 500;
//   cfg.state.genesis_balances = {};
//   cfg.state.dpos = nullopt;
//   vector<dev::KeyPair> keys;
//   keys.reserve(NUM_ACCS);
//   for (size_t i = 0; i < NUM_ACCS; ++i) {
//     auto const& k = keys.emplace_back(dev::KeyPair::create());
//     cfg.state.genesis_balances[k.address()] = numeric_limits<u256>::max() / NUM_ACCS;
//   }
//   cfg.state.execution_options.disable_gas_fee = false;
//   init();
//   constexpr auto TRX_GAS = 100000;
//   advance({
//       {0, 13, 0, TRX_GAS, {}, keys[10].secret(), keys[10].address()},
//       {0, 11300, 0, TRX_GAS, {}, keys[102].secret(), keys[44].address()},
//       {0, 1040, 0, TRX_GAS, {}, keys[122].secret(), keys[50].address()},
//   });
//   advance({});
//   advance({
//       {0, 0, 0, TRX_GAS, {}, keys[2].secret(), keys[1].address()},
//       {0, 131, 0, TRX_GAS, {}, keys[133].secret(), keys[133].address()},
//   });
//   advance({
//       {0, 100441, 0, TRX_GAS, {}, keys[177].secret(), keys[431].address()},
//       {0, 2300, 0, TRX_GAS, {}, keys[131].secret(), keys[343].address()},
//       {0, 130, 0, TRX_GAS, {}, keys[11].secret(), keys[23].address()},
//   });
//   advance({});
//   advance({
//       {0, 100431, 0, TRX_GAS, {}, keys[135].secret(), keys[232].address()},
//       {0, 13411, 0, TRX_GAS, {}, keys[112].secret(), keys[34].address()},
//       {0, 130, 0, TRX_GAS, {}, keys[133].secret(), keys[233].address()},
//       {0, 343434, 0, TRX_GAS, {}, keys[13].secret(), keys[213].address()},
//       {0, 131313, 0, TRX_GAS, {}, keys[405].secret(), keys[344].address()},
//       {0, 143430, 0, TRX_GAS, {}, keys[331].secret(), keys[420].address()},
//       {0, 1313145, 0, TRX_GAS, {}, keys[345].secret(), keys[134].address()},
//   });
// }

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
