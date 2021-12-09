#include "final_chain/contract_interface.hpp"

#include <libdevcore/SHA3.h>

#include "final_chain/data.hpp"

namespace taraxa::final_chain {
ContractInterface::ContractInterface(addr_t contract_addr, std::shared_ptr<FinalChain> final_chain)
    : contract_addr_(std::move(contract_addr)), final_chain_(std::move(final_chain)) {}

template <typename Result, typename... Params>
Result ContractInterface::call(const string& function, const Params&... args) {
  return unpack<Result>(finalChainCall(pack<Params...>(function, args...)));
}

bytes ContractInterface::finalChainCall(bytes data) {
  state_api::EVMTransaction trx;
  trx.input = std::move(data);
  // return final_chain_->call(trx,
  //     final_chain_->last_block_number(),
  //     state_api::ExecutionOptions{
  //         true, //disable_nonce_check
  //         false, //disable_gas_fee
  //     }).code_retval;
  return final_chain_->call(trx).code_retval;
}

template <typename Result>
Result ContractInterface::unpack(const bytes& /*data*/) {
  Result r;
  return r;
}

bytes ContractInterface::getFunction(const string& function) {
  return dev::sha3(function.c_str()).ref().cropped(0, 4).toBytes();
}

}  // namespace taraxa::final_chain