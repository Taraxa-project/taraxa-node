#include "final_chain/data.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa::final_chain {

h256 const& BlockHeader::uncles_hash() { return EmptyRLPListSHA3(); }

Nonce const& BlockHeader::nonce() { return EmptyNonce(); }

u256 const& BlockHeader::difficulty() { return ZeroU256(); }

h256 const& BlockHeader::mix_hash() { return ZeroHash(); }

RLP_FIELDS_DEFINE(BlockHeader, hash, parent_hash, author, state_root, transactions_root, receipts_root, log_bloom,
                  number, gas_limit, gas_used, timestamp, extra_data)

void BlockHeader::ethereum_rlp(dev::RLPStream& encoding) const {
  util::rlp_tuple(encoding, parent_hash, BlockHeader::uncles_hash(), author, state_root, transactions_root,
                  receipts_root, log_bloom, BlockHeader::difficulty(), number, gas_limit, gas_used, timestamp,
                  extra_data, BlockHeader::mix_hash(), BlockHeader::nonce());
}

RLP_FIELDS_DEFINE(LogEntry, address, topics, data)

LogBloom LogEntry::bloom() const {
  LogBloom ret;
  ret.shiftBloom<3>(sha3(address.ref()));
  for (auto t : topics) {
    ret.shiftBloom<3>(sha3(t.ref()));
  }
  return ret;
}

RLP_FIELDS_DEFINE(TransactionReceipt, status_code, gas_used, cumulative_gas_used, logs, new_contract_address)

LogBloom TransactionReceipt::bloom() const {
  LogBloom ret;
  for (auto const& l : logs) {
    ret |= l.bloom();
  }
  return ret;
}

RLP_FIELDS_DEFINE(TransactionLocation, blk_n, index)

}  // namespace taraxa::final_chain