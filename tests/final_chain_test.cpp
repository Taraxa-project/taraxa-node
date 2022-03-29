#include "final_chain/final_chain.hpp"

#include <optional>
#include <vector>

#include "common/constants.hpp"
#include "config/chain_config.hpp"
#include "final_chain/trie_common.hpp"
#include "util_test/gtest.hpp"
#include "util_test/util.hpp"
#include "vote/vote.hpp"

namespace taraxa::final_chain {
using namespace std;

struct advance_check_opts {
  bool dont_assume_no_logs = 0;
  bool dont_assume_all_trx_success = 0;
};

struct FinalChainTest : WithDataDir {
  shared_ptr<DbStorage> db{new DbStorage(data_dir / "db")};
  Config cfg = ChainConfig::predefined("test").final_chain;
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
    PbftBlock pbft_block(blk_hash_t(), blk_hash_t(), blk_hash_t(), 1, addr_t::random(),
                         dev::KeyPair::create().secret());
    std::vector<std::shared_ptr<Vote>> votes;
    SyncBlock sync_block(std::make_shared<PbftBlock>(std::move(pbft_block)), votes);
    sync_block.dag_blocks.push_back(dag_blk);
    sync_block.transactions = trxs;

    auto batch = db->createWriteBatch();
    db->savePeriodData(sync_block, batch);

    db->commitWriteBatch(batch);

    auto result = SUT->finalize(std::move(sync_block), {dag_blk.getHash()}).get();
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
    EXPECT_EQ(blk_h.author, pbft_block.getBeneficiary());
    EXPECT_EQ(blk_h.timestamp, pbft_block.getTimestamp());
    EXPECT_EQ(receipts.size(), trxs.size());
    EXPECT_EQ(blk_h.transactions_root,
              trieRootOver(
                  trxs.size(), [&](auto i) { return dev::rlp(i); }, [&](auto i) { return trxs[i].rlp(); }));
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

TEST_F(FinalChainTest, genesis_balances) {
  cfg.state.dpos = nullopt;
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr_t::random()] = 0;
  cfg.state.genesis_balances[addr_t::random()] = 1000;
  cfg.state.genesis_balances[addr_t::random()] = 100000;
  init();
}

TEST_F(FinalChainTest, update_state_config) {
  init();
  cfg.state.hardforks.fix_genesis_fork_block = 2222222;
  SUT->update_state_config(cfg.state);
}

TEST_F(FinalChainTest, contract) {
  auto sender_keys = dev::KeyPair::create();
  auto const& addr = sender_keys.address();
  auto const& sk = sender_keys.secret();
  cfg.state.genesis_balances = {};
  cfg.state.genesis_balances[addr] = 100000;
  cfg.state.dpos = nullopt;
  cfg.state.execution_options.disable_nonce_check = true;
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
  Transaction trx(0, 100, 0, 0, dev::fromHex(contract_deploy_code), sk);
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
        0,
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
        Transaction(0, 11, 0, 0,
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
  cfg.state.genesis_balances = {};
  cfg.state.dpos = nullopt;
  vector<dev::KeyPair> keys;
  keys.reserve(NUM_ACCS);
  for (size_t i = 0; i < NUM_ACCS; ++i) {
    auto const& k = keys.emplace_back(dev::KeyPair::create());
    cfg.state.genesis_balances[k.address()] = numeric_limits<u256>::max() / NUM_ACCS;
  }
  cfg.state.execution_options.disable_gas_fee = false;
  init();
  constexpr auto TRX_GAS = 100000;
  advance({
      {0, 13, 0, TRX_GAS, {}, keys[10].secret(), keys[10].address()},
      {0, 11300, 0, TRX_GAS, {}, keys[102].secret(), keys[44].address()},
      {0, 1040, 0, TRX_GAS, {}, keys[122].secret(), keys[50].address()},
  });
  advance({});
  advance({
      {0, 0, 0, TRX_GAS, {}, keys[2].secret(), keys[1].address()},
      {0, 131, 0, TRX_GAS, {}, keys[133].secret(), keys[133].address()},
  });
  advance({
      {0, 100441, 0, TRX_GAS, {}, keys[177].secret(), keys[431].address()},
      {0, 2300, 0, TRX_GAS, {}, keys[131].secret(), keys[343].address()},
      {0, 130, 0, TRX_GAS, {}, keys[11].secret(), keys[23].address()},
  });
  advance({});
  advance({
      {0, 100431, 0, TRX_GAS, {}, keys[135].secret(), keys[232].address()},
      {0, 13411, 0, TRX_GAS, {}, keys[112].secret(), keys[34].address()},
      {0, 130, 0, TRX_GAS, {}, keys[134].secret(), keys[233].address()},
      {0, 343434, 0, TRX_GAS, {}, keys[13].secret(), keys[213].address()},
      {0, 131313, 0, TRX_GAS, {}, keys[405].secret(), keys[344].address()},
      {0, 143430, 0, TRX_GAS, {}, keys[331].secret(), keys[420].address()},
      {0, 1313145, 0, TRX_GAS, {}, keys[345].secret(), keys[134].address()},
  });
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
