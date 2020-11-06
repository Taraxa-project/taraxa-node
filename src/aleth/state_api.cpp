#include "state_api.hpp"

namespace taraxa::aleth {
using namespace dev;
using namespace eth;
using namespace rpc;
using namespace std;

struct StateAPIImpl : virtual Eth::StateAPI {
  shared_ptr<FinalChain> final_chain;

  auto call_internal(BlockNumber _blockNumber, TransactionSkeleton const& trx, bool free_gas) const {
    return final_chain->call(
        {
            trx.from,
            trx.gasPrice.value_or(0),
            trx.to,
            trx.nonce.value_or(0),
            trx.value,
            trx.gas.value_or(0),
            trx.data,
        },
        _blockNumber,
        // TODO options should cascade
        state_api::ExecutionOptions{true, free_gas});
  }

  bytes call(BlockNumber _blockNumber, TransactionSkeleton const& trx) const override { return call_internal(_blockNumber, trx, false).CodeRet; }

  uint64_t estimateGas(BlockNumber _blockNumber, TransactionSkeleton const& trx) const override {
    return call_internal(_blockNumber, trx, true).GasUsed;
  }

  u256 balanceAt(Address _a, BlockNumber n) const override {
    if (auto acc = final_chain->get_account(_a, n)) {
      return acc->Balance;
    }
    return 0;
  }

  uint64_t nonceAt(Address _a, BlockNumber n) const override {
    if (auto acc = final_chain->get_account(_a, n)) {
      return acc->Nonce;
    }
    return 0;
  }

  bytes codeAt(Address _a, BlockNumber n) const override { return final_chain->get_code(_a, n); }

  h256 stateRootAt(Address _a, BlockNumber n) const override {
    if (auto acc = final_chain->get_account(_a, n)) {
      return acc->storage_root_eth();
    }
    return {};
  }

  u256 stateAt(Address _a, u256 _l, BlockNumber n) const override { return final_chain->get_account_storage(_a, _l, n); }
};

unique_ptr<Eth::StateAPI> NewStateAPI(shared_ptr<FinalChain> final_chain) {
  auto ret = u_ptr(new StateAPIImpl);
  ret->final_chain = move(final_chain);
  return ret;
}

}  // namespace taraxa::aleth
