#include "storage/migration/final_chain_header.hpp"

#include "final_chain/data.hpp"

namespace taraxa::storage::migration {

FinalChainHeader::FinalChainHeader(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string FinalChainHeader::id() { return "FinalChainHeader"; }

uint32_t FinalChainHeader::dbVersion() { return 1; }

struct OldHeader : final_chain::BlockHeader {
  RLP_FIELDS_DEFINE_INPLACE(hash, parent_hash, author, state_root, transactions_root, receipts_root, log_bloom, number,
                            gas_limit, gas_used, timestamp, total_reward, extra_data)
};

void FinalChainHeader::migrate(logger::Logger& log) {
  auto orig_col = DbStorage::Columns::final_chain_blk_by_number;
  auto copied_col = db_->copyColumn(db_->handle(orig_col), orig_col.name() + "-copy");

  if (copied_col == nullptr) {
    LOG(log) << "Migration " << id() << " skipped: Unable to copy " << orig_col.name() << " column";
    return;
  }

  auto it = db_->getColumnIterator(copied_col.get());
  it->SeekToFirst();
  if (!it->Valid()) {
    LOG(log) << "No blocks to migrate";
    return;
  }

  uint64_t batch_size = 500000000;
  for (; it->Valid(); it->Next()) {
    uint64_t period;
    memcpy(&period, it->key().data(), sizeof(uint64_t));
    std::string raw = it->value().ToString();
    auto header = std::make_shared<OldHeader>();
    header->rlp(dev::RLP(raw));
    auto newBytes = header->serializeForDB();
    db_->insert(batch_, copied_col.get(), period, newBytes);
    if (batch_.GetDataSize() > batch_size) {
      db_->commitWriteBatch(batch_);
    }
  }
  // commit the left over batch
  db_->commitWriteBatch(batch_);

  db_->replaceColumn(orig_col, std::move(copied_col));
}
}  // namespace taraxa::storage::migration