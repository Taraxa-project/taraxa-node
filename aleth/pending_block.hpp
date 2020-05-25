#ifndef TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_
#define TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_

#include <libweb3jsonrpc/Eth.h>

#include <shared_mutex>

#include "db_storage.hpp"
#include "transaction.hpp"
#include "util/range_view.hpp"

namespace taraxa::aleth::pending_block {
using namespace std;
using namespace dev;
using namespace eth;
using namespace util;

class PendingBlock : public virtual rpc::Eth::PendingBlock {
  BlockHeader block_header;
  shared_ptr<DbStorage> db;
  mutable shared_mutex mu;

 public:
  PendingBlock(uint64_t number, addr_t const& author,
               h256 const& curr_block_hash, decltype(db) db);

  BlockDetails details() const override;
  unsigned int transactionsCount() const override;
  unsigned int transactionsCount(Address const& from) const override;
  Transactions transactions() const override;
  optional<eth::Transaction> transaction(unsigned index) const override;
  h256s transactionHashes() const override;
  BlockHeader header() const override;

  void add_transactions(RangeView<h256> const& pending_trx_hashes);
  void advance(DbStorage::BatchPtr batch, h256 const& curr_block_hash,
               RangeView<h256> const& executed_trx_hashes);
};

}  // namespace taraxa::aleth::pending_block

namespace taraxa::aleth {
using pending_block::PendingBlock;
}

#endif  // TARAXA_NODE_ALETH_PENDING_BLOCK_HPP_
