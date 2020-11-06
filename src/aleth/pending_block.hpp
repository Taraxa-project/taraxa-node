#ifndef TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_
#define TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_

#include <libweb3jsonrpc/Eth.h>

#include "db_storage.hpp"
#include "util/range_view.hpp"

namespace taraxa::aleth {

struct PendingBlock : virtual dev::rpc::Eth::PendingBlock {
  virtual ~PendingBlock() {}
  virtual void add_transactions(util::RangeView<dev::h256> const& pending_trx_hashes) = 0;
  virtual void advance(DbStorage::BatchPtr batch, dev::h256 const& curr_block_hash, util::RangeView<dev::h256> const& executed_trx_hashes) = 0;
};

std::unique_ptr<PendingBlock> NewPendingBlock(uint64_t number, addr_t const& author, dev::h256 const& curr_block_hash, std::shared_ptr<DbStorage> db);

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_
