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
  std::vector<addr_t> validators;
  for (int i = 0; i < 50; i++) {
    validators.push_back(dev::FixedHash<20>::random());
  }
  std::vector<addr_t> delegators;
  for (int i = 0; i < 200; i++) {
    delegators.push_back(dev::FixedHash<20>::random());
  }
  auto sender_keys = dev::KeyPair::create();
  auto const& addr = sender_keys.address();
  auto const& sk = sender_keys.secret();
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr] = 1000000000000;
  for (const auto& acc : validators) {
    cfg.state.genesis_balances[acc] = 100000;
  }
  // cfg.state.dpos = nullopt;
  auto& dpos = cfg.state.dpos.emplace();
  dpos.eligibility_balance_threshold = 10;
  dpos.genesis_state[addr][addr] = dpos.eligibility_balance_threshold;
  for (const auto& acc : validators) {
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
      "6080604052610631806100136000396000f3fe60806040526004361061007b5760003560e01c80639161babb1161004e5780639161babb14"
      "6101f0578063bb2eb4d214610226578063ce8695cb1461023b578063cfa40aed146102655761007b565b8063437c315f146100805780635d"
      "a4a1d31461014c5780637e9ff31f14610178578063896b897d146101bd575b600080fd5b34801561008c57600080fd5b506100b360048036"
      "0360208110156100a357600080fd5b50356001600160a01b031661027a565b60405180806020018060200183810383528581815181526020"
      "0191508051906020019060200280838360005b838110156100f75781810151838201526020016100df565b50505050905001838103825284"
      "818151815260200191508051906020019060200280838360005b8381101561013657818101518382015260200161011e565b505050509050"
      "0194505050505060405180910390f35b34801561015857600080fd5b506101766004803603602081101561016f57600080fd5b50356104aa"
      "565b005b34801561018457600080fd5b506101ab6004803603602081101561019b57600080fd5b50356001600160a01b0316610526565b60"
      "408051918252519081900360200190f35b3480156101c957600080fd5b506101ab600480360360208110156101e057600080fd5b50356001"
      "600160a01b0316610538565b6101766004803603606081101561020657600080fd5b506001600160a01b0381358116916020810135909116"
      "906040013561054a565b34801561023257600080fd5b506101ab6105b7565b34801561024757600080fd5b50610176600480360360208110"
      "1561025e57600080fd5b50356105bc565b34801561027157600080fd5b506101ab6105c1565b6001600160a01b0381166000908152600160"
      "205260409020546060908190819067ffffffffffffffff811180156102b057600080fd5b5060405190808252806020026020018201604052"
      "80156102da578160200160208202803683370190505b506001600160a01b03851660009081526001602052604090205490915060609067ff"
      "ffffffffffffff8111801561031057600080fd5b5060405190808252806020026020018201604052801561033a5781602001602082028036"
      "83370190505b5090506000805b6001600160a01b03871660009081526001602052604090205481101561049e576001600160a01b03871660"
      "009081526002602090815260408083206001909252822080549192918490811061039257fe5b600091825260208083209091015460016001"
      "60a01b031683528201929092526040019020546103c657600190910190610496565b6001600160a01b038716600090815260016020526040"
      "90208054829081106103ea57fe5b9060005260206000200160009054906101000a90046001600160a01b0316848383038151811061041657"
      "fe5b6001600160a01b0392831660209182029290920181019190915290881660009081526002825260408082206001909352812080548490"
      "811061045457fe5b60009182526020808320909101546001600160a01b031683528201929092526040019020548351849084840390811061"
      "048957fe5b6020026020010181815250505b600101610341565b50919350915050915091565b336000908152600460205260409020548015"
      "61051357600054810182111580156104d8575060005481038210155b6105135760405162461bcd60e51b8152600401808060200182810382"
      "5260348152602001806105c86034913960400191505060405180910390fd5b5033600090815260046020526040902055565b600460205260"
      "00908152604090205481565b60036020526000908152604090205481565b6001600160a01b03928316600081815260016020818152604080"
      "842080549384018155845281842090920180546001600160a01b031916969097169586179096558282526002865280822094825293855283"
      "81208054840190559081526003909352912080549091019055565b606481565b600055565b6000548156fe43616e2774206368616e676520"
      "76616c756520666f72206d6f7265207468616e206d617850657263656e746167654368616e6765a26469706673582212207b1fa15f202c98"
      "6b0e3c9fc6d800e85412647ab21a13808685d013bcac12d0dd64736f6c63430006080033";
  Transaction trx(0, 200, 0, 0, dev::fromHex(contract_deploy_code), sk);
  auto result = advance({trx});
  auto contract_addr = result->trx_receipts[0].new_contract_address;
  EXPECT_EQ(contract_addr, dev::right160(dev::sha3(dev::rlpList(addr, 0))));

  // advance({
  //     Transaction(1, 200, 0, 0, ContractInterface::pack("set(address,uint256)", addr, 1), sk, contract_addr),
  // });
  int i = 1;
  for (const auto& acc : validators) {
    Transactions trxs;
    for (const auto& acc1 : delegators) {
      trxs.push_back(Transaction(i++, 200, 0, 0,
                                 ContractInterface::pack("delegateTokens(address,address,uint256)", acc, acc1, 1), sk,
                                 contract_addr));
    }
    advance(trxs);
  }

  auto get = [&](const auto& acc) {
    auto ret = SUT->call({
        addr,
        0,
        contract_addr,
        0,
        0,
        0,
        ContractInterface::pack("getDelegatorsAndDelegations(address)", acc),
    });
    return dev::toHexPrefixed(ret.code_retval);
  };

  auto start = high_resolution_clock::now();
  for (const auto& acc : validators) {
    get(acc);
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start);
  cout << duration.count() << endl;

  start = high_resolution_clock::now();
  for (const auto& acc : validators) {
    SUT->dpos_eligible_vote_count(1, acc);
  }
  stop = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(stop - start);
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
