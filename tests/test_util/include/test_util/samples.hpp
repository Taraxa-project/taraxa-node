#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <string>

#include "common/lazy.hpp"
#include "dag/dag_block.hpp"
#include "test_util/test_util.hpp"
#include "transaction/transaction.hpp"

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

bool sendTrx(uint64_t count, unsigned port, dev::Secret secret);

SharedTransactions createSignedTrxSamples(unsigned start, unsigned num, secret_t const &sk,
                                          bytes data = dev::fromHex("00FEDCBA9876543210000000"));

std::vector<std::shared_ptr<DagBlock>> createMockDag0(const blk_hash_t &genesis);

std::vector<std::shared_ptr<DagBlock>> createMockDag1(const blk_hash_t &genesis);

}  // namespace taraxa::core_tests::samples
