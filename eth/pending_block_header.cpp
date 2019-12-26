#include "pending_block_header.hpp"

#include <libethashseal/Ethash.h>

namespace taraxa::eth::pending_block_header {
using dev::eth::Ethash;

PendingBlockHeader::PendingBlockHeader(int64_t number, h256 const& parent_hash,
                                       Address const& author, int64_t timestamp,
                                       u256 const& gas_limit,
                                       bytes const& extra_data,
                                       u256 const& difficulty,
                                       h256 const& mix_hash, Nonce nonce) {
  setNumber(number);
  setParentHash(parent_hash);
  setAuthor(author);
  setTimestamp(timestamp);
  setGasLimit(gas_limit);
  if (!extra_data.empty()) {
    setExtraData(extra_data);
  }
  setDifficulty(difficulty);
  Ethash::setMixHash(*this, mix_hash);
  Ethash::setNonce(*this, nonce);
}

void PendingBlockHeader::complete(u256 const& gas_used,
                                  LogBloom const& log_bloom,
                                  h256 const& trx_root,
                                  h256 const& receipts_root,
                                  h256 const& state_root) {
  setGasUsed(gas_used);
  setLogBloom(log_bloom);
  setRoots(trx_root, receipts_root, dev::EmptyListSHA3, state_root);
}

}  // namespace taraxa::eth::pending_block_header