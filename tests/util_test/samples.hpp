#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <string>

#include "common/lazy.hpp"
#include "dag/dag_block.hpp"
#include "transaction/transaction.hpp"
#include "util.hpp"

namespace taraxa::core_tests::samples {

static string const greeter_contract_code =
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

class TxGenerator {
 public:
  // this function guarantees uniqueness for generated values
  // scoped to the class instance
  auto getRandomUniqueSenderSecret() const {
    std::unique_lock l(mutex_);
    auto secret = secret_t::random();
    while (!used_secrets_.insert(secret.makeInsecure()).second) {
      secret = secret_t::random();
    }
    return secret;
  }

  auto getSerialTrxWithSameSender(uint trx_num, uint64_t const &start_nonce, val_t const &value,
                                  addr_t const &receiver = addr_t::random()) const {
    SharedTransactions trxs;
    for (auto i = start_nonce; i < start_nonce + trx_num; ++i) {
      trxs.emplace_back(std::make_shared<Transaction>(i, value, 0, TEST_TX_GAS_LIMIT,
                                                      dev::fromHex("00FEDCBA9876543210000000"),
                                                      getRandomUniqueSenderSecret(), receiver));
    }
    return trxs;
  }

 private:
  mutable std::mutex mutex_;
  mutable std::unordered_set<dev::FixedHash<secret_t::size>> used_secrets_;
};

inline auto const TX_GEN = Lazy([] { return TxGenerator(); });

inline bool sendTrx(uint64_t count, unsigned port) {
  auto pattern = R"(
      curl --silent -m 10 --output /dev/null -d \
      '{
        "jsonrpc": "2.0",
        "method": "send_coin_transaction",
        "id": "0",
        "params": [
          {
            "nonce": 0,
            "value": 0,
            "gas": "%s",
            "gas_price": "%s",
            "receiver": "%s",
            "secret": "%s"
          }
        ]
      }' 0.0.0.0:%s
    )";
  for (uint64_t i = 0; i < count; ++i) {
    auto retcode = system(fmt(pattern, val_t(TEST_TX_GAS_LIMIT), val_t(0), addr_t::random(),
                              samples::TX_GEN->getRandomUniqueSenderSecret().makeInsecure(), port)
                              .c_str());
    if (retcode != 0) {
      return false;
    }
  }
  return true;
}

struct TestAccount {
  TestAccount() = default;
  TestAccount(int id, std::string const &sk, std::string const &pk, std::string const &addr)
      : id(id), sk(sk), pk(pk), addr(addr) {}
  friend std::ostream;
  int id;
  std::string sk;
  std::string pk;
  std::string addr;
};

inline std::ostream &operator<<(std::ostream &strm, TestAccount const &acc) {
  strm << "sk: " << acc.sk << "\npk: " << acc.pk << "\naddr: " << acc.addr << std::endl;
  return strm;
}

inline std::map<int, TestAccount> createTestAccountTable(std::string const &filename) {
  std::map<int, TestAccount> acc_table;
  std::ifstream file;
  file.open(filename);
  if (file.fail()) {
    std::cerr << "Error! Cannot open " << filename << std::endl;
    return acc_table;
  }

  std::string id, sk, pk, addr;

  while (file >> id >> sk >> pk >> addr) {
    if (id.empty()) break;
    if (sk.empty()) break;
    if (pk.empty()) break;
    if (addr.empty()) break;

    int idx = std::stoi(id);
    acc_table[idx] = TestAccount(idx, sk, pk, addr);
  }
  return acc_table;
}

inline SharedTransactions createSignedTrxSamples(unsigned start, unsigned num, secret_t const &sk,
                                                 bytes data = dev::fromHex("00FEDCBA9876543210000000")) {
  assert(start + num < std::numeric_limits<unsigned>::max());
  SharedTransactions trxs;
  for (auto i = start; i < num; ++i) {
    trxs.emplace_back(std::make_shared<Transaction>(i, i * 100, 0, 100000, data, sk, addr_t::random()));
  }
  return trxs;
}

inline std::vector<DagBlock> createMockDagBlkSamples(unsigned pivot_start, unsigned blk_num, unsigned trx_start,
                                                     unsigned trx_len, unsigned trx_overlap) {
  assert(pivot_start + blk_num < std::numeric_limits<unsigned>::max());
  std::vector<DagBlock> blks;
  unsigned trx = trx_start;
  for (auto i = pivot_start; i < blk_num; ++i) {
    blk_hash_t pivot(i);
    blk_hash_t hash(i + 1);
    vec_trx_t trxs;
    for (unsigned i = 0; i < trx_len; ++i, trx++) {
      trxs.emplace_back(trx_hash_t(trx));
    }
    for (unsigned i = 0; i < trx_overlap; ++i) {
      trx--;
    }

    DagBlock blk(blk_hash_t(pivot),                              // pivot
                 level_t(0),                                     // level
                 {blk_hash_t(2), blk_hash_t(3), blk_hash_t(4)},  // tips
                 trxs,                                           // trxs
                 sig_t(7777),                                    // sig
                 blk_hash_t(hash),                               // hash
                 addr_t(12345));                                 // sender

    blks.emplace_back(blk);
  }
  return blks;
}

//
inline std::vector<DagBlock> createMockDag0(
    std::string genesis = "0000000000000000000000000000000000000000000000000000000000000000") {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk2(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk3(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {}, secret_t::random());
  DagBlock blk4(blk1.getHash(),  // pivot
                2,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk5(blk1.getHash(),    // pivot
                2,                 // level
                {blk2.getHash()},  // tips
                {}, secret_t::random());
  DagBlock blk6(blk3.getHash(),  // pivot
                2,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk7(blk5.getHash(),    // pivot
                3,                 // level
                {blk6.getHash()},  // tips
                {}, secret_t::random());
  DagBlock blk8(blk5.getHash(),  // pivot
                3,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk9(blk6.getHash(),  // pivot
                3,               // level
                {},              // tips
                {}, secret_t::random());
  DagBlock blk10(blk7.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk11(blk7.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk12(blk9.getHash(),  // pivot
                 4,               // level
                 {},              // tips
                 {}, secret_t::random());
  DagBlock blk13(blk10.getHash(),  // pivot
                 5,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk14(blk11.getHash(),    // pivot
                 5,                  // level
                 {blk12.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk15(blk13.getHash(),    // pivot
                 6,                  // level
                 {blk14.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk16(blk13.getHash(),  // pivot
                 6,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk17(blk12.getHash(),  // pivot
                 5,                // level
                 {},               // tips
                 {}, secret_t::random());
  DagBlock blk18(blk15.getHash(),                                     // pivot
                 7,                                                   // level
                 {blk8.getHash(), blk16.getHash(), blk17.getHash()},  // tips
                 {}, secret_t::random());
  DagBlock blk19(blk18.getHash(),  // pivot
                 8,                // level
                 {},               // tips
                 {}, secret_t::random());
  blks.emplace_back(dummy);
  blks.emplace_back(blk1);
  blks.emplace_back(blk2);
  blks.emplace_back(blk3);
  blks.emplace_back(blk4);
  blks.emplace_back(blk5);
  blks.emplace_back(blk6);
  blks.emplace_back(blk7);
  blks.emplace_back(blk8);
  blks.emplace_back(blk9);
  blks.emplace_back(blk10);
  blks.emplace_back(blk11);
  blks.emplace_back(blk12);
  blks.emplace_back(blk13);
  blks.emplace_back(blk14);
  blks.emplace_back(blk15);
  blks.emplace_back(blk16);
  blks.emplace_back(blk17);
  blks.emplace_back(blk18);
  blks.emplace_back(blk19);

  return blks;
}

//
inline std::vector<DagBlock> createMockDag1(
    std::string genesis = "0000000000000000000000000000000000000000000000000000000000000000") {
  std::vector<DagBlock> blks;
  DagBlock dummy;
  DagBlock blk1(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(1),        // hash
                addr_t(123));

  DagBlock blk2(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(2),        // hash
                addr_t(123));
  DagBlock blk3(blk_hash_t(genesis),  // pivot
                1,                    // level
                {},                   // tips
                {},                   // trxs
                sig_t(0),             // sig
                blk_hash_t(3),        // hash
                addr_t(123));
  DagBlock blk4(blk_hash_t(1),  // pivot
                2,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(4),  // hash
                addr_t(123));
  DagBlock blk5(blk_hash_t(1),    // pivot
                2,                // level
                {blk_hash_t(2)},  // tips
                {},               // trxs
                sig_t(0),         // sig
                blk_hash_t(5),    // hash
                addr_t(123));
  DagBlock blk6(blk_hash_t(3),  // pivot
                2,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(6),  // hash
                addr_t(123));
  DagBlock blk7(blk_hash_t(5),    // pivot
                3,                // level
                {blk_hash_t(6)},  // tips
                {},               // trxs
                sig_t(0),         // sig
                blk_hash_t(7),    // hash
                addr_t(123));
  DagBlock blk8(blk_hash_t(5),  // pivot
                3,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(8),  // hash
                addr_t(123));
  DagBlock blk9(blk_hash_t(6),  // pivot
                3,              // level
                {},             // tips
                {},             // trxs
                sig_t(0),       // sig
                blk_hash_t(9),  // hash
                addr_t(123));
  DagBlock blk10(blk_hash_t(7),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(10),  // hash
                 addr_t(123));
  DagBlock blk11(blk_hash_t(7),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(11),  // hash
                 addr_t(123));
  DagBlock blk12(blk_hash_t(9),   // pivot
                 4,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(12),  // hash
                 addr_t(123));
  DagBlock blk13(blk_hash_t(10),  // pivot
                 5,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(13),  // hash
                 addr_t(123));
  DagBlock blk14(blk_hash_t(11),    // pivot
                 5,                 // level
                 {blk_hash_t(12)},  // tips
                 {},                // trxs
                 sig_t(0),          // sig
                 blk_hash_t(14),    // hash
                 addr_t(123));
  DagBlock blk15(blk_hash_t(13),    // pivot
                 6,                 // level
                 {blk_hash_t(14)},  // tips
                 {},                // trxs
                 sig_t(0),          // sig
                 blk_hash_t(15),    // hash
                 addr_t(123));
  DagBlock blk16(blk_hash_t(13),  // pivot
                 6,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(16),  // hash
                 addr_t(123));
  DagBlock blk17(blk_hash_t(12),  // pivot
                 5,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(17),  // hash
                 addr_t(123));
  DagBlock blk18(blk_hash_t(15),                                   // pivot
                 7,                                                // level
                 {blk_hash_t(8), blk_hash_t(16), blk_hash_t(17)},  // tips
                 {},                                               // trxs
                 sig_t(0),                                         // sig
                 blk_hash_t(18),                                   // hash
                 addr_t(123));
  DagBlock blk19(blk_hash_t(18),  // pivot
                 8,               // level
                 {},              // tips
                 {},              // trxs
                 sig_t(0),        // sig
                 blk_hash_t(19),  // hash
                 addr_t(123));
  blks.emplace_back(dummy);
  blks.emplace_back(blk1);
  blks.emplace_back(blk2);
  blks.emplace_back(blk3);
  blks.emplace_back(blk4);
  blks.emplace_back(blk5);
  blks.emplace_back(blk6);
  blks.emplace_back(blk7);
  blks.emplace_back(blk8);
  blks.emplace_back(blk9);
  blks.emplace_back(blk10);
  blks.emplace_back(blk11);
  blks.emplace_back(blk12);
  blks.emplace_back(blk13);
  blks.emplace_back(blk14);
  blks.emplace_back(blk15);
  blks.emplace_back(blk16);
  blks.emplace_back(blk17);
  blks.emplace_back(blk18);
  blks.emplace_back(blk19);

  return blks;
}

}  // namespace taraxa::core_tests::samples
