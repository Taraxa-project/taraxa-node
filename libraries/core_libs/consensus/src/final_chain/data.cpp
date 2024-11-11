#include "final_chain/data.hpp"

#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"

namespace taraxa::final_chain {

const h256& BlockHeader::uncles_hash() { return EmptyRLPListSHA3(); }

const Nonce& BlockHeader::nonce() { return EmptyNonce(); }

const u256& BlockHeader::difficulty() { return ZeroU256(); }

const h256& BlockHeader::mix_hash() { return ZeroHash(); }

std::shared_ptr<BlockHeader> BlockHeader::from_rlp(const dev::RLP& rlp) {
  auto ret = std::make_shared<BlockHeader>();
  ret->rlp(rlp);
  dev::RLPStream encoding;
  ret->ethereum_rlp(encoding);
  ret->size = encoding.out().size();
  return ret;
}

// TODO[2888]: remove hash field to not store it in the db
RLP_FIELDS_DEFINE(BlockHeader, hash, parent_hash, author, state_root, transactions_root, receipts_root, log_bloom,
                  number, gas_limit, gas_used, timestamp, total_reward, extra_data)

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

}  // namespace taraxa::final_chain