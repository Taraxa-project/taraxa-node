#include "final_chain/data.hpp"

#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>

#include "common/constants.hpp"
#include "pbft/pbft_block.hpp"

namespace taraxa::final_chain {

dev::bytes BlockHeaderData::serializeForDB() const { return util::rlp_enc(*this); }

RLP_FIELDS_DEFINE(BlockHeaderData, hash, parent_hash, state_root, transactions_root, receipts_root, log_bloom, gas_used,
                  total_reward)

BlockHeader::BlockHeader(std::string&& raw_header_data)
    : BlockHeaderData(util::rlp_dec<BlockHeaderData>(dev::RLP(raw_header_data))) {}

BlockHeader::BlockHeader(std::string&& raw_header_data_, const PbftBlock& pbft_, uint64_t gas_limit_)
    : BlockHeader(std::move(raw_header_data_)) {
  setFromPbft(pbft_);
  gas_limit = gas_limit_;
}

void BlockHeader::setFromPbft(const PbftBlock& pbft) {
  author = pbft.getBeneficiary();
  timestamp = pbft.getTimestamp();
  number = pbft.getPeriod();
  extra_data = pbft.getExtraDataRlp();
}

h256 const& BlockHeader::unclesHash() { return EmptyRLPListSHA3(); }

Nonce const& BlockHeader::nonce() { return EmptyNonce(); }

u256 const& BlockHeader::difficulty() { return ZeroU256(); }

h256 const& BlockHeader::mixHash() { return ZeroHash(); }

dev::bytes&& BlockHeader::ethereumRlp() const {
  dev::RLPStream rlp_strm;
  util::rlp_tuple(rlp_strm, parent_hash, BlockHeader::unclesHash(), author, state_root, transactions_root,
                  receipts_root, log_bloom, BlockHeader::difficulty(), number, gas_limit, gas_used, timestamp,
                  extra_data, BlockHeader::mixHash(), BlockHeader::nonce());
  return rlp_strm.invalidate();
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