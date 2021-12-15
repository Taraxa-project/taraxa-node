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
      // contract Delegation {
      //     uint256 public index_zero = 10;
      //     // constants is not in storage
      //     uint256 constant public ONE_PERCENT = 100;
      //     uint256 public index_one = 11;
      //     uint256 public maxPercentageChange;
    
      //     // mapping from producer to array of delegators that delegated to that producer
      //     // producer => [deleagator1, delegator2, ...]
      //     mapping (address => address[]) private _delegators;
      //     // producer => delegator => amount
      //     mapping (address => mapping (address => uint256)) private _delegations;
      //     // producer => total_delegation
      //     mapping (address => uint256) public delegatedCoins;
      //     // should not be zero for registered delegatee
      //     mapping (address => uint256) public delegateePercents;

      //     constructor() payable {}

      //     function setMaxPercentageChange(uint256 v) public payable {
      //         maxPercentageChange = v;
      //     }

      //     function setPercentage(address sender, uint256 v) public payable {
      //         // delay before it would be finally applied should be added
      //         uint256 currentPercentage = delegateePercents[sender];
      //         if (currentPercentage != 0) {
      //             require(v <= (currentPercentage + maxPercentageChange) && v >= (currentPercentage -
      //             maxPercentageChange), "Can't change value for more than maxPercentageChange");
      //         }
      //         delegateePercents[sender] = v;
      //     }

      //     function delegateTokens(address sender, address delegationAccount, uint256 amount) public payable {
      //         _delegators[delegationAccount].push(sender);
      //         _delegations[delegationAccount][sender] += amount;
      //         delegatedCoins[delegationAccount] += amount;
      //     }

      //     function revokeTokens(address sender, address delegationAccount, uint256 amount) public payable {
      //         require(_delegations[delegationAccount][sender] >= amount);
      //         _delegations[delegationAccount][sender] -= amount;
      //         delegatedCoins[delegationAccount] -= amount;
      //     }

      //     function getDelegatorsAndDelegations(address user) public view returns (address[] memory, uint256[] memory)
      //     {
      //         address[] memory addresses = new address[](_delegators[user].length);
      //         uint256[] memory values = new uint256[](_delegators[user].length);
      //         uint256 skip = 0;

      //         for (uint i = 0; i < _delegators[user].length; i++) {
      //             if (_delegations[user][_delegators[user][i]] == 0) {
      //                 skip++;
      //                 continue;
      //             }
      //             addresses[i - skip] = _delegators[user][i];
      //             values[i - skip] = _delegations[user][_delegators[user][i]];
      //         }
      //         return (addresses, values);
      //     }
      // }
      "6080604052600a600055600b6001556111018061001d6000396000f3fe60806040526004361061009c5760003560e01c8063896b897d1161"
      "0064578063896b897d1461017f5780639161babb146101bc578063bb2eb4d2146101d8578063ce8695cb14610203578063cfa40aed146102"
      "1f578063dc2c170b1461024a5761009c565b8063437c315f146100a157806369161600146100df5780636b89da54146100fb5780636fb0bd"
      "25146101175780637e9ff31f14610142575b600080fd5b3480156100ad57600080fd5b506100c860048036038101906100c39190610b8f56"
      "5b610275565b6040516100d6929190610db8565b60405180910390f35b6100f960048036038101906100f49190610c0f565b610710565b00"
      "5b61011560048036038101906101109190610bbc565b61080f565b005b34801561012357600080fd5b5061012c610986565b604051610139"
      "9190610e0f565b60405180910390f35b34801561014e57600080fd5b5061016960048036038101906101649190610b8f565b61098c565b60"
      "40516101769190610e0f565b60405180910390f35b34801561018b57600080fd5b506101a660048036038101906101a19190610b8f565b61"
      "09a4565b6040516101b39190610e0f565b60405180910390f35b6101d660048036038101906101d19190610bbc565b6109bc565b005b3480"
      "156101e457600080fd5b506101ed610b4a565b6040516101fa9190610e0f565b60405180910390f35b61021d600480360381019061021891"
      "90610c4f565b610b4f565b005b34801561022b57600080fd5b50610234610b59565b6040516102419190610e0f565b60405180910390f35b"
      "34801561025657600080fd5b5061025f610b5f565b60405161026c9190610e0f565b60405180910390f35b6060806000600360008573ffff"
      "ffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002080"
      "54905067ffffffffffffffff8111156102d6576102d561101a565b5b60405190808252806020026020018201604052801561030457816020"
      "01602082028036833780820191505090505b5090506000600360008673ffffffffffffffffffffffffffffffffffffffff1673ffffffffff"
      "ffffffffffffffffffffffffffffff1681526020019081526020016000208054905067ffffffffffffffff8111156103655761036461101a"
      "565b5b6040519080825280602002602001820160405280156103935781602001602082028036833780820191505090505b5090506000805b"
      "600360008873ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081"
      "5260200160002080549050811015610701576000600460008973ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffff"
      "ffffffffffffffffffffffff1681526020019081526020016000206000600360008b73ffffffffffffffffffffffffffffffffffffffff16"
      "73ffffffffffffffffffffffffffffffffffffffff168152602001908152602001600020848154811061047857610477610feb565b5b9060"
      "005260206000200160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffff"
      "ffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081526020016000205414156104f75781806104ef90"
      "610f73565b9250506106ee565b600360008873ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffff"
      "ffffffffff168152602001908152602001600020818154811061054857610547610feb565b5b906000526020600020016000905490610100"
      "0a900473ffffffffffffffffffffffffffffffffffffffff168483836105809190610f03565b8151811061059157610590610feb565b5b60"
      "2002602001019073ffffffffffffffffffffffffffffffffffffffff16908173ffffffffffffffffffffffffffffffffffffffff16815250"
      "50600460008873ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190"
      "81526020016000206000600360008a73ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffff"
      "ffff168152602001908152602001600020838154811061065d5761065c610feb565b5b9060005260206000200160009054906101000a9004"
      "73ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffff"
      "ffffffffffffffffff168152602001908152602001600020548383836106d09190610f03565b815181106106e1576106e0610feb565b5b60"
      "20026020010181815250505b80806106f990610f73565b91505061039a565b50828294509450505050915091565b6000600660008473ffff"
      "ffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002054"
      "9050600081146107c6576002548161076a9190610ead565b82111580156107865750600254816107829190610f03565b8210155b6107c557"
      "6040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016107bc90610def565b604051809103"
      "90fd5b5b81600660008573ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152"
      "60200190815260200160002081905550505050565b80600460008473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffff"
      "ffffffffffffffffffffffffffff16815260200190815260200160002060008573ffffffffffffffffffffffffffffffffffffffff1673ff"
      "ffffffffffffffffffffffffffffffffffffff16815260200190815260200160002054101561089857600080fd5b80600460008473ffffff"
      "ffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081526020016000206000"
      "8573ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001908152602001"
      "60002060008282546109249190610f03565b9250508190555080600560008473ffffffffffffffffffffffffffffffffffffffff1673ffff"
      "ffffffffffffffffffffffffffffffffffff168152602001908152602001600020600082825461097a9190610f03565b9250508190555050"
      "5050565b60005481565b60066020528060005260406000206000915090505481565b60056020528060005260406000206000915090505481"
      "565b600360008373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001"
      "908152602001600020839080600181540180825580915050600190039060005260206000200160009091909190916101000a81548173ffff"
      "ffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550806004600084"
      "73ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160"
      "002060008573ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1681526020019081"
      "526020016000206000828254610ae89190610ead565b9250508190555080600560008473ffffffffffffffffffffffffffffffffffffffff"
      "1673ffffffffffffffffffffffffffffffffffffffff1681526020019081526020016000206000828254610b3e9190610ead565b92505081"
      "905550505050565b606481565b8060028190555050565b60025481565b60015481565b600081359050610b748161109d565b92915050565b"
      "600081359050610b89816110b4565b92915050565b600060208284031215610ba557610ba4611049565b5b6000610bb384828501610b6556"
      "5b91505092915050565b600080600060608486031215610bd557610bd4611049565b5b6000610be386828701610b65565b9350506020610b"
      "f486828701610b65565b9250506040610c0586828701610b7a565b9150509250925092565b60008060408385031215610c2657610c256110"
      "49565b5b6000610c3485828601610b65565b9250506020610c4585828601610b7a565b9150509250929050565b600060208284031215610c"
      "6557610c64611049565b5b6000610c7384828501610b7a565b91505092915050565b6000610c888383610cac565b60208301905092915050"
      "565b6000610ca08383610d9a565b60208301905092915050565b610cb581610f37565b82525050565b6000610cc682610e4a565b610cd081"
      "85610e7a565b9350610cdb83610e2a565b8060005b83811015610d0c578151610cf38882610c7c565b9750610cfe83610e60565b92505060"
      "0181019050610cdf565b5085935050505092915050565b6000610d2482610e55565b610d2e8185610e8b565b9350610d3983610e3a565b80"
      "60005b83811015610d6a578151610d518882610c94565b9750610d5c83610e6d565b925050600181019050610d3d565b5085935050505092"
      "915050565b6000610d84603483610e9c565b9150610d8f8261104e565b604082019050919050565b610da381610f69565b82525050565b61"
      "0db281610f69565b82525050565b60006040820190508181036000830152610dd28185610cbb565b90508181036020830152610de6818461"
      "0d19565b90509392505050565b60006020820190508181036000830152610e0881610d77565b9050919050565b6000602082019050610e24"
      "6000830184610da9565b92915050565b6000819050602082019050919050565b6000819050602082019050919050565b6000815190509190"
      "50565b600081519050919050565b6000602082019050919050565b6000602082019050919050565b60008282526020820190509291505056"
      "5b600082825260208201905092915050565b600082825260208201905092915050565b6000610eb882610f69565b9150610ec383610f6956"
      "5b9250827fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff03821115610ef857610ef7610fbc565b5b8282"
      "01905092915050565b6000610f0e82610f69565b9150610f1983610f69565b925082821015610f2c57610f2b610fbc565b5b828203905092"
      "915050565b6000610f4282610f49565b9050919050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60"
      "00819050919050565b6000610f7e82610f69565b91507fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82"
      "1415610fb157610fb0610fbc565b5b600182019050919050565b7f4e487b7100000000000000000000000000000000000000000000000000"
      "000000600052601160045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260"
      "045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b60"
      "0080fd5b7f43616e2774206368616e67652076616c756520666f72206d6f7265207468616e60008201527f206d617850657263656e746167"
      "654368616e6765000000000000000000000000602082015250565b6110a681610f37565b81146110b157600080fd5b50565b6110bd81610f"
      "69565b81146110c857600080fd5b5056fea2646970667358221220d2fcdcfe9d433eb333df741abd297a3dadc61c3ffe0d1f8140f175e91d"
      "3877d064736f6c63430008070033";
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

  for (uint i = 0; i < 4; ++i) {
    std::cout << "at " << i << " index: " << SUT->get_account_storage(*contract_addr, dev::u256(i)) << std::endl;
  }

  auto start = high_resolution_clock::now();
  for (const auto& acc : validators) {
    get(acc);
  }

  auto stop = high_resolution_clock::now();
  auto res = start.time_since_epoch();
  auto duration = duration_cast<milliseconds>(stop - start);
  cout << "getDelegatorsAndDelegations(address): " << duration.count() << "ms" << endl;

  start = high_resolution_clock::now();
  for (const auto& acc : delegators) {
    SUT->dpos_eligible_vote_count(1, acc);
  }
  stop = high_resolution_clock::now();
  res = start.time_since_epoch();
  duration = duration_cast<milliseconds>(stop - start);
  cout << "dpos_eligible_vote_count: " << duration.count() << "ms" << endl;

  start = high_resolution_clock::now();
  const auto delegated_coins_index = "0000000000000000000000000000000000000000000000000000000000000005";
  auto to64string = [](const addr_t& addr) { return "000000000000000000000000" + addr.toString(); };
  for (const auto& acc : delegators) {
    std::cout << "key: " << to64string(acc) + delegated_coins_index << std::endl;
    auto key = dev::sha3(dev::h512(to64string(acc) + delegated_coins_index));
    auto ret = SUT->get_account_storage(*contract_addr, key);
    std::cout << ret << std::endl;
  }
  stop = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(stop - start);
  cout << "get storage: " << duration.count() << "ms" << endl;
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
