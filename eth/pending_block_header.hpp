#ifndef TARAXA_NODE_ETH_PENDING_BLOCK_HEADER_HPP_
#define TARAXA_NODE_ETH_PENDING_BLOCK_HEADER_HPP_

#include <libethcore/BlockHeader.h>

namespace taraxa::eth::eth_service {
class EthService;
}

namespace taraxa::eth::pending_block_header {
using dev::Address;
using dev::bytes;
using dev::h256;
using dev::u256;
using dev::eth::BlockHeader;
using dev::eth::LogBloom;
using dev::eth::Nonce;

struct PendingBlockHeader : private BlockHeader {
  friend class ::taraxa::eth::eth_service::EthService;

  PendingBlockHeader(int64_t number, h256 const& parent_hash,
                     Address const& author, int64_t timestamp,
                     u256 const& gas_limit, bytes const& extra_data,
                     u256 const& difficulty, h256 const& mix_hash, Nonce nonce);

  using BlockHeader::author;
  using BlockHeader::difficulty;
  using BlockHeader::gasLimit;
  using BlockHeader::number;
  using BlockHeader::timestamp;

 private:
  void complete(u256 const& gas_used, LogBloom const& log_bloom,
                h256 const& trx_root, h256 const& receipts_root,
                h256 const& state_root);
};

}  // namespace taraxa::eth::pending_block_header

#endif  // TARAXA_NODE_ETH_PENDING_BLOCK_HEADER_HPP_
