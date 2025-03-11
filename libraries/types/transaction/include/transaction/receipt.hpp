#pragma once

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

namespace taraxa {

using LogBloom = dev::h2048;
using LogBlooms = std::vector<LogBloom>;

struct LogEntry {
  Address address;
  h256s topics;
  bytes data;

  HAS_RLP_FIELDS

  LogBloom bloom() const;
};

using LogEntries = std::vector<LogEntry>;

struct TransactionReceipt {
  uint8_t status_code = 0;
  uint64_t gas_used = 0;
  uint64_t cumulative_gas_used = 0;
  LogEntries logs;
  std::optional<Address> new_contract_address;

  HAS_RLP_FIELDS

  LogBloom bloom() const;
};

using TransactionReceipts = std::vector<TransactionReceipt>;

struct TransactionLocation {
  EthBlockNumber period = 0;
  uint32_t position = 0;
  bool is_system = false;

  static TransactionLocation fromRlp(dev::RLP&& rlp) {
    TransactionLocation res;
    auto it = rlp.begin();
    res.period = (*it++).toInt<PbftPeriod>();
    res.position = (*it++).toInt<uint32_t>();
    if (rlp.itemCount() == 3) {
      res.is_system = (*it++).toInt<bool>();
    }
    return res;
  }
};

using SharedTransactionReceipts = std::shared_ptr<std::vector<TransactionReceipt>>;

}  // namespace taraxa