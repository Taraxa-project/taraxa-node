#include "pending_block.hpp"

namespace taraxa::aleth::pending_block {

PendingBlock::PendingBlock(uint64_t number, addr_t const& author,
                           h256 const& curr_block_hash, decltype(db) db) {
  BlockHeaderFields header_fields;
  header_fields.m_number = number;
  header_fields.m_parentHash = curr_block_hash;
  header_fields.m_author = author;
  header_fields.m_timestamp = dev::utcTime();
  block_header = BlockHeader(header_fields);
}

BlockDetails PendingBlock::details() const {
  shared_lock l(mu);
  return {block_header.number(), 0, block_header.parentHash(), 0};
}

unsigned int PendingBlock::transactionsCount() const {
  return transactionHashes().size();
}

unsigned int PendingBlock::transactionsCount(Address const& from) const {
  unsigned ret = 0;
  for (auto const& trx : transactions()) {
    if (trx.from() == from) {
      ++ret;
    }
  }
  return ret;
}

Transactions PendingBlock::transactions() const {
  Transactions ret;
  for (auto const& h : transactionHashes()) {
    auto trx_rlp = db->getTransactionRaw(h);
    if (!trx_rlp.empty()) {
      ret.emplace_back(trx_rlp, CheckTransaction::None);
    }
  }
  return ret;
}

optional<eth::Transaction> PendingBlock::transaction(unsigned index) const {
  auto trxs = transactions();
  return index < trxs.size() ? optional(trxs[index]) : nullopt;
}

h256s PendingBlock::transactionHashes() const {
  h256s ret;
  db->forEach(DbStorage::Columns::pending_transactions,
              [&](auto const& k, auto const& _) {
                ret.emplace_back(h256(k.data()));
                return true;
              });
  return ret;
}

BlockHeader PendingBlock::header() const {
  shared_lock l(mu);
  return block_header;
}

void PendingBlock::add_transactions(RangeView<h256> const& pending_trx_hashes) {
  pending_trx_hashes.for_each([&, this](auto const& h) {
    db->insert(DbStorage::Columns::pending_transactions,
               DbStorage::toSlice(h.ref()), "_");
  });
}

void PendingBlock::advance(DbStorage::BatchPtr batch,
                           h256 const& curr_block_hash,
                           RangeView<h256> const& executed_trx_hashes) {
  unique_lock l(mu);
  BlockHeaderFields header_fields;
  header_fields.m_number = block_header.number() + 1;
  header_fields.m_parentHash = curr_block_hash;
  header_fields.m_author = block_header.author();
  header_fields.m_timestamp = dev::utcTime();
  block_header = BlockHeader(header_fields);
  executed_trx_hashes.for_each([&, this](auto const& h) {
    db->batch_delete(batch, DbStorage::Columns::pending_transactions,
                     DbStorage::toSlice(h.ref()));
  });
}

}  // namespace taraxa::aleth::pending_block
