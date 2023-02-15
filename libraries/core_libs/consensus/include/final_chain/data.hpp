#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>

#include <algorithm>

#include "common/constants.hpp"
#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "final_chain/state_api_data.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
class PbftBlock;
}

namespace taraxa::final_chain {

/** @addtogroup FinalChain
 * @{
 */

using LogBloom = dev::h2048;
using LogBlooms = std::vector<LogBloom>;
using Nonce = dev::h64;

struct BlockHeader {
  h256 hash;
  h256 parent_hash;
  h256 state_root;
  h256 transactions_root;
  h256 receipts_root;
  LogBloom log_bloom;
  EthBlockNumber number = 0;
  uint64_t gas_limit = 0;
  uint64_t gas_used = 0;
  bytes extra_data;
  uint64_t timestamp = 0;
  Address author;
  u256 total_reward;

  HAS_RLP_FIELDS

  static h256 const& uncles_hash();

  static Nonce const& nonce();

  static u256 const& difficulty();

  static h256 const& mix_hash();

  void ethereum_rlp(dev::RLPStream& encoding) const;
};

static constexpr auto c_bloomIndexSize = 16;
static constexpr auto c_bloomIndexLevels = 2;

using BlocksBlooms = std::array<LogBloom, c_bloomIndexSize>;

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
  EthBlockNumber blk_n = 0;
  uint64_t index = 0;

  HAS_RLP_FIELDS
};

struct NewBlock {
  addr_t author;
  uint64_t timestamp;
  std::vector<h256> dag_blk_hashes;
  h256 hash;
};

struct FinalizationResult : NewBlock {
  std::shared_ptr<BlockHeader const> final_chain_blk;
  SharedTransactions trxs;
  TransactionReceipts trx_receipts;
};

/** @} */

}  // namespace taraxa::final_chain
