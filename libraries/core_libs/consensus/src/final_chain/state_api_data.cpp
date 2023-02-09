#include "final_chain/state_api_data.hpp"

#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"

namespace taraxa::state_api {

TaraxaEVMError::TaraxaEVMError(std::string&& type, const std::string& msg)
    : runtime_error(msg), type(std::move(type)) {}

h256 const& Account::storage_root_eth() const { return storage_root_hash ? storage_root_hash : EmptyRLPListSHA3(); }

RLP_FIELDS_DEFINE(EVMBlock, author, gas_limit, time, difficulty)
RLP_FIELDS_DEFINE(EVMTransaction, from, gas_price, to, nonce, value, gas, input)
RLP_FIELDS_DEFINE(UncleBlock, number, author)
RLP_FIELDS_DEFINE(LogRecord, address, topics, data)
RLP_FIELDS_DEFINE(ExecutionResult, code_retval, new_contract_addr, logs, gas_used, code_err, consensus_err)
RLP_FIELDS_DEFINE(StateTransitionResult, execution_results, state_root)
RLP_FIELDS_DEFINE(Account, nonce, balance, storage_root_hash, code_hash, code_size)
RLP_FIELDS_DEFINE(TrieProof, value, nodes)
RLP_FIELDS_DEFINE(Proof, account_proof, storage_proofs)
RLP_FIELDS_DEFINE(StateDescriptor, blk_num, state_root)
RLP_FIELDS_DEFINE(Tracing, vmTrace, trace, stateDiff)
}  // namespace taraxa::state_api