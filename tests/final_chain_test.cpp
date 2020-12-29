#include "chain/final_chain.hpp"

#include <libdevcore/TrieHash.h>

#include <optional>
#include <vector>

#include "chain/chain_config.hpp"
#include "util_test/gtest.hpp"

namespace taraxa::final_chain {
using namespace std;

struct advance_check_opts {
  bool dont_assume_no_logs = 0;
  bool dont_assume_all_trx_success = 0;
};

struct FinalChainTest : WithDataDir {
  shared_ptr<DbStorage> db{new DbStorage(data_dir / "db")};
  FinalChain::Config cfg = ChainConfig::predefined().final_chain;
  unique_ptr<FinalChain> SUT;
  bool assume_only_toplevel_transfers = true;
  unordered_map<addr_t, u256> expected_balances;
  uint64_t expected_blk_num = 0;

  void init() {
    SUT = NewFinalChain(db, cfg);
    for (auto const& [addr, _] : cfg.state.genesis_balances) {
      auto acc_actual = SUT->get_account(addr);
      ASSERT_TRUE(acc_actual);
      auto expected_bal = cfg.state.effective_genesis_balance(addr);
      ASSERT_EQ(acc_actual->Balance, expected_bal);
      expected_balances[addr] = expected_bal;
    }
  }

  auto advance(Transactions const& trxs, advance_check_opts opts = {}) {
    auto batch = db->createWriteBatch();
    auto author = addr_t::random();
    uint64_t timestamp = chrono::high_resolution_clock::now().time_since_epoch().count();
    auto result = SUT->advance(batch, author, timestamp, trxs);
    db->commitWriteBatch(batch);
    SUT->advance_confirm();
    ++expected_blk_num;
    auto const& blk_h = result.new_header;
    EXPECT_EQ(blk_h.hash(), SUT->get_last_block()->hash());
    EXPECT_EQ(blk_h.parentHash(), SUT->blockHeader(expected_blk_num - 1).hash());
    EXPECT_EQ(blk_h.number(), expected_blk_num);
    EXPECT_EQ(blk_h.author(), author);
    EXPECT_EQ(blk_h.timestamp(), timestamp);
    EXPECT_EQ(result.receipts.size(), trxs.size());
    EXPECT_EQ(blk_h.transactionsRoot(),
              trieRootOver(
                  trxs.size(), [&](auto i) { return rlp(i); }, [&](auto i) { return trxs[i].rlp(); }));
    EXPECT_EQ(blk_h.receiptsRoot(),
              trieRootOver(
                  trxs.size(), [&](auto i) { return rlp(i); }, [&](auto i) { return result.receipts[i].rlp(); }));
    EXPECT_EQ(blk_h.gasLimit(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(blk_h.extraData(), bytes());
    EXPECT_EQ(blk_h.nonce(), Nonce());
    EXPECT_EQ(blk_h.difficulty(), 0);
    EXPECT_EQ(blk_h.mixHash(), h256());
    EXPECT_EQ(blk_h.sha3Uncles(), EmptyListSHA3);
    EXPECT_EQ(blk_h.gasUsed(), result.receipts.empty() ? 0 : result.receipts.back().cumulativeGasUsed());
    LogBloom expected_block_log_bloom;
    unordered_map<addr_t, u256> expected_balance_changes;
    unordered_set<addr_t> all_addrs_w_changed_balance;
    for (size_t i = 0; i < trxs.size(); ++i) {
      auto const& trx = trxs[i];
      auto const& r = result.receipts[i];
      EXPECT_EQ(r.rlp(), SUT->transactionReceipt(trx.sha3()).rlp());
      EXPECT_EQ(trx.rlp(), SUT->transaction(trx.sha3()).rlp());
      if (assume_only_toplevel_transfers && trx.value() != 0 && r.statusCode() == 1) {
        auto const& sender = trx.from();
        auto const& sender_bal = expected_balances[sender] -= trx.value();
        auto const& receiver = trx.isCreation() ? r.contractAddress() : trx.to();
        all_addrs_w_changed_balance.insert(sender);
        all_addrs_w_changed_balance.insert(receiver);
        auto const& receiver_bal = expected_balances[receiver] += trx.value();
        if (SUT->get_account(sender)->CodeSize == 0) {
          expected_balance_changes[sender] = sender_bal;
        }
        if (SUT->get_account(receiver)->CodeSize == 0) {
          expected_balance_changes[receiver] = receiver_bal;
        }
      }
      EXPECT_TRUE(r.hasStatusCode());
      if (!opts.dont_assume_all_trx_success) {
        EXPECT_EQ(r.statusCode(), 1);
      }
      if (!opts.dont_assume_no_logs) {
        EXPECT_EQ(r.log().size(), 0);
        EXPECT_EQ(r.bloom(), LogBloom());
      }
      expected_block_log_bloom |= r.bloom();
      auto r_from_db = SUT->localisedTransactionReceipt(trxs[i].sha3());
      EXPECT_EQ(r_from_db.contractAddress(), result.state_transition_result.ExecutionResults[i].NewContractAddr);
      EXPECT_EQ(r_from_db.from(), trx.from());
      EXPECT_EQ(r_from_db.blockHash(), blk_h.hash());
      EXPECT_EQ(r_from_db.blockNumber(), blk_h.number());
      EXPECT_EQ(r_from_db.transactionIndex(), i);
      EXPECT_EQ(r_from_db.hash(), trx.sha3());
      EXPECT_EQ(r_from_db.to(), trx.to());
      EXPECT_EQ(r_from_db.bloom(), r.bloom());
      EXPECT_TRUE(r_from_db.hasStatusCode());
      EXPECT_EQ(r_from_db.statusCode(), r.statusCode());
      EXPECT_EQ(r_from_db.gasUsed(), result.state_transition_result.ExecutionResults[i].GasUsed);
      EXPECT_EQ(r_from_db.gasUsed(),
                i == 0 ? r.cumulativeGasUsed() : r.cumulativeGasUsed() - result.receipts[i - 1].cumulativeGasUsed());
    }
    expected_block_log_bloom.shiftBloom<3>(sha3(blk_h.author().ref()));
    EXPECT_EQ(blk_h.logBloom(), expected_block_log_bloom);
    EXPECT_EQ(blk_h.stateRoot(), result.state_transition_result.StateRoot);
    if (assume_only_toplevel_transfers) {
      for (auto const& addr : all_addrs_w_changed_balance) {
        EXPECT_EQ(SUT->get_account(addr)->Balance, expected_balances[addr]);
      }
    }
    return result;
  }
};

TEST_F(FinalChainTest, genesis_balances) {
  cfg.state.dpos = nullopt;
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr_t::random()] = 0;
  cfg.state.genesis_balances[addr_t::random()] = 1000;
  cfg.state.genesis_balances[addr_t::random()] = 100000;
  init();
}

TEST_F(FinalChainTest, contract) {
  auto sender_keys = KeyPair::create();
  auto const& addr = sender_keys.address();
  auto const& sk = sender_keys.secret();
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr] = 100000;
  cfg.state.dpos = nullopt;
  init();
  static string const contract_deploy_code =
      // pragma solidity ^0.6.8;
      // contract Greeter {
      //    string public greeting;
      //
      //    constructor() public payable {
      //       greeting = 'Hello';
      //    }
      //
      //    function setGreeting(string memory _greeting) public payable {
      //       greeting = _greeting;
      //    }
      //
      //    function greet() view public returns (string memory) {
      //       return greeting;
      //    }
      //}
      "0x60806040526040518060400160405280600581526020017f48656c6c6f000000000000"
      "000000000000000000000000000000000000000000815250600090805190602001906100"
      "4f929190610055565b506100fa565b828054600181600116156101000203166002900490"
      "600052602060002090601f016020900481019282601f1061009657805160ff1916838001"
      "1785556100c4565b828001600101855582156100c4579182015b828111156100c3578251"
      "8255916020019190600101906100a8565b5b5090506100d191906100d5565b5090565b61"
      "00f791905b808211156100f35760008160009055506001016100db565b5090565b90565b"
      "610449806101096000396000f3fe6080604052600436106100345760003560e01c8063a4"
      "13686214610039578063cfae3217146100f4578063ef690cc014610184575b600080fd5b"
      "6100f26004803603602081101561004f57600080fd5b8101908080359060200190640100"
      "00000081111561006c57600080fd5b82018360208201111561007e57600080fd5b803590"
      "602001918460018302840111640100000000831117156100a057600080fd5b9190808060"
      "1f0160208091040260200160405190810160405280939291908181526020018383808284"
      "37600081840152601f19601f820116905080830192505050505050509192919290505050"
      "610214565b005b34801561010057600080fd5b5061010961022e565b6040518080602001"
      "828103825283818151815260200191508051906020019080838360005b83811015610149"
      "57808201518184015260208101905061012e565b50505050905090810190601f16801561"
      "01765780820380516001836020036101000a031916815260200191505b50925050506040"
      "5180910390f35b34801561019057600080fd5b506101996102d0565b6040518080602001"
      "828103825283818151815260200191508051906020019080838360005b838110156101d9"
      "5780820151818401526020810190506101be565b50505050905090810190601f16801561"
      "02065780820380516001836020036101000a031916815260200191505b50925050506040"
      "5180910390f35b806000908051906020019061022a92919061036e565b5050565b606060"
      "008054600181600116156101000203166002900480601f01602080910402602001604051"
      "908101604052809291908181526020018280546001816001161561010002031660029004"
      "80156102c65780601f1061029b576101008083540402835291602001916102c6565b8201"
      "91906000526020600020905b8154815290600101906020018083116102a957829003601f"
      "168201915b5050505050905090565b600080546001816001161561010002031660029004"
      "80601f016020809104026020016040519081016040528092919081815260200182805460"
      "0181600116156101000203166002900480156103665780601f1061033b57610100808354"
      "040283529160200191610366565b820191906000526020600020905b8154815290600101"
      "9060200180831161034957829003601f168201915b505050505081565b82805460018160"
      "0116156101000203166002900490600052602060002090601f016020900481019282601f"
      "106103af57805160ff19168380011785556103dd565b828001600101855582156103dd57"
      "9182015b828111156103dc5782518255916020019190600101906103c1565b5b50905061"
      "03ea91906103ee565b5090565b61041091905b8082111561040c57600081600090555060"
      "01016103f4565b5090565b9056fea264697066735822122004585b83cf41cfb8af886165"
      "0679892acca0561c1a8ab45ce31c7fdb15a67b7764736f6c63430006080033";
  dev::eth::Transaction trx(100, 0, 0, dev::fromHex(contract_deploy_code), 0, sk);
  auto result = advance({trx});
  auto contract_addr = result.state_transition_result.ExecutionResults[0].NewContractAddr;
  auto greet = [&] {
    auto ret = SUT->call({
        addr,
        0,
        contract_addr,
        0,
        0,
        0,
        // greet()
        dev::fromHex("0xcfae3217"),
    });
    return dev::toHexPrefixed(ret.CodeRet);
  };
  ASSERT_EQ(greet(),
            // "Hello"
            "0x0000000000000000000000000000000000000000000000000000000000000020"
            "000000000000000000000000000000000000000000000000000000000000000548"
            "656c6c6f000000000000000000000000000000000000000000000000000000");
  {
    dev::eth::Transaction trx(11, 0, 0, contract_addr,
                              // setGreeting("Hola")
                              dev::fromHex("0xa4136862000000000000000000000000000000000000000000000000"
                                           "00000000000000200000000000000000000000000000000000000000000"
                                           "000000000000000000004486f6c61000000000000000000000000000000"
                                           "00000000000000000000000000"),
                              0, sk);
    advance({trx});
  }
  ASSERT_EQ(greet(),
            // "Hola"
            "0x000000000000000000000000000000000000000000000000000000000000002000"
            "00000000000000000000000000000000000000000000000000000000000004486f"
            "6c6100000000000000000000000000000000000000000000000000000000");
}

TEST_F(FinalChainTest, coin_transfers) {
  constexpr size_t NUM_ACCS = 500;
  cfg.state.genesis_balances = {};
  cfg.state.dpos = nullopt;
  vector<KeyPair> keys;
  keys.reserve(NUM_ACCS);
  for (size_t i = 0; i < NUM_ACCS; ++i) {
    auto const& k = keys.emplace_back(KeyPair::create());
    cfg.state.genesis_balances[k.address()] = numeric_limits<u256>::max() / NUM_ACCS;
  }
  cfg.state.execution_options.disable_gas_fee = false;
  init();
  constexpr auto TRX_GAS = 100000;
  advance({
      {13, 0, TRX_GAS, keys[10].address(), {}, 0, keys[10].secret()},
      {11300, 0, TRX_GAS, keys[44].address(), {}, 0, keys[102].secret()},
      {1040, 0, TRX_GAS, keys[50].address(), {}, 0, keys[122].secret()},
  });
  advance({});
  advance({
      {0, 0, TRX_GAS, keys[1].address(), {}, 0, keys[2].secret()},
      {131, 0, TRX_GAS, keys[133].address(), {}, 0, keys[133].secret()},
  });
  advance({
      {100441, 0, TRX_GAS, keys[431].address(), {}, 0, keys[177].secret()},
      {2300, 0, TRX_GAS, keys[343].address(), {}, 0, keys[131].secret()},
      {130, 0, TRX_GAS, keys[23].address(), {}, 0, keys[11].secret()},
  });
  advance({});
  advance({
      {100431, 0, TRX_GAS, keys[232].address(), {}, 0, keys[135].secret()},
      {13411, 0, TRX_GAS, keys[34].address(), {}, 0, keys[112].secret()},
      {130, 0, TRX_GAS, keys[233].address(), {}, 0, keys[133].secret()},
      {343434, 0, TRX_GAS, keys[213].address(), {}, 0, keys[13].secret()},
      {131313, 0, TRX_GAS, keys[344].address(), {}, 0, keys[405].secret()},
      {143430, 0, TRX_GAS, keys[420].address(), {}, 0, keys[331].secret()},
      {1313145, 0, TRX_GAS, keys[134].address(), {}, 0, keys[345].secret()},
  });
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
