#include "storage/migration/transaction_hashes.hpp"

namespace taraxa::storage::migration {
struct OldTransactionsHashes {
  std::string serialized_;
  size_t count_;

  explicit OldTransactionsHashes(std::string serialized)
      : serialized_(std::move(serialized)), count_(serialized_.size() / dev::h256::size) {}
  dev::h256 get(size_t i) const {
    return dev::h256(reinterpret_cast<const uint8_t*>(serialized_.data() + i * dev::h256::size),
                     dev::h256::ConstructFromPointer);
  }
  size_t count() const { return count_; }
};

TransactionHashes::TransactionHashes(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string TransactionHashes::id() { return "TransactionHashes"; }

uint32_t TransactionHashes::dbVersion() { return 1; }

void TransactionHashes::migrate() {
  auto it = db_->getColumnIterator(DB::Columns::final_chain_transaction_hashes_by_blk_number);

  // Get and save data in new format for all blocks
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    ::taraxa::TransactionHashes new_data;
    auto old_data = std::make_unique<OldTransactionsHashes>(it->value().ToString());
    new_data.reserve(old_data->count());
    for (size_t i = 0; i < new_data.capacity(); ++i) {
      new_data.emplace_back(old_data->get(i));
    }
    db_->insert(batch_, DB::Columns::final_chain_transaction_hashes_by_blk_number, it->key(), dev::rlp(new_data));
  }
}
}  // namespace taraxa::storage::migration