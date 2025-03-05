#include "transaction/receipt.hpp"

#include <libdevcore/SHA3.h>

namespace taraxa {

RLP_FIELDS_DEFINE(LogEntry, address, topics, data)

LogBloom LogEntry::bloom() const {
  LogBloom ret;
  ret.shiftBloom<3>(dev::sha3(address.ref()));
  for (auto t : topics) {
    ret.shiftBloom<3>(dev::sha3(t.ref()));
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

}  // namespace taraxa
