#include "pending_block.hpp"

#include <shared_mutex>

#include "transaction.hpp"

namespace taraxa::aleth {
using namespace dev;
using namespace eth;
using namespace std;
using namespace util;

// TODO better performance, cache
// TODO better consistency with the last block
struct PendingBlockImpl : PendingBlock {
  BlockHeader block_header;
  shared_ptr<DbStorage> db;
  mutable shared_mutex mu;

  BlockDetails details() const override {
    shared_lock l(mu);
    return {block_header.number(), 0, block_header.parentHash(), 0};
  }

  uint64_t transactionsCount() const override { return transactionHashes().size(); }

  uint64_t transactionsCount(Address const& from) const override {
    uint64_t ret = 0;
    for (auto const& trx : transactions()) {
      if (trx.from() == from) {
        ++ret;
      }
    }
    return ret;
  }

  Transactions transactions() const override {
    Transactions ret;
    for (auto const& h : transactionHashes()) {
      auto trx_rlp = db->getTransactionRaw(h);
      if (!trx_rlp.empty()) {
        ret.emplace_back(trx_rlp, CheckTransaction::None);
      }
    }
    return ret;
  }

  optional<eth::Transaction> transaction(unsigned index) const override {
    auto trxs = transactions();
    return index < trxs.size() ? optional(trxs[index]) : nullopt;
  }

  h256s transactionHashes() const override {
    h256s ret;
    db->forEach(DbStorage::Columns::pending_transactions, [&](auto const& k, auto const& _) {
      ret.emplace_back(k.data(), h256::FromBinary);
      return true;
    });
    return ret;
  }

  BlockHeader header() const override {
    shared_lock l(mu);
    return block_header;
  }

  void add_transactions(RangeView<h256> const& pending_trx_hashes) override {
    pending_trx_hashes.for_each([&, this](auto const& h) { db->insert(DbStorage::Columns::pending_transactions, DbStorage::toSlice(h.ref()), "_"); });
  }

  void advance(DbStorage::BatchPtr batch, h256 const& curr_block_hash, RangeView<h256> const& executed_trx_hashes) override {
    unique_lock l(mu);
    BlockHeaderFields header_fields;
    header_fields.m_number = block_header.number() + 1;
    header_fields.m_parentHash = curr_block_hash;
    header_fields.m_author = block_header.author();
    header_fields.m_timestamp = dev::utcTime();
    block_header = BlockHeader(header_fields);
    executed_trx_hashes.for_each(
        [&, this](auto const& h) { db->batch_delete(batch, DbStorage::Columns::pending_transactions, DbStorage::toSlice(h.ref())); });
  }
};

unique_ptr<PendingBlock> NewPendingBlock(uint64_t number, addr_t const& author, h256 const& curr_block_hash, shared_ptr<DbStorage> db) {
  auto ret = u_ptr(new PendingBlockImpl);
  BlockHeaderFields header_fields;
  header_fields.m_number = number;
  header_fields.m_parentHash = curr_block_hash;
  header_fields.m_author = author;
  header_fields.m_timestamp = dev::utcTime();
  ret->block_header = {header_fields};
  ret->db = db;
  return ret;
}

}  // namespace taraxa::aleth
