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

void TransactionHashes::migrate() {
  auto last_blk_num =
      db_->lookup_int<EthBlockNumber>(final_chain::DBMetaKeys::LAST_NUMBER, DB::Columns::final_chain_meta);

  auto batch = db_->createWriteBatch();
  // Get and save data in new format for all blocks
  for (uint64_t p = 1; p <= last_blk_num; ++p) {
    ::taraxa::TransactionHashes new_data;
    auto old_data = std::make_unique<OldTransactionsHashes>(
        db_->lookup(p, DB::Columns::final_chain_transaction_hashes_by_blk_number));
    new_data.reserve(old_data->count());
    for (size_t i = 0; i < new_data.capacity(); ++i) {
      new_data.emplace_back(old_data->get(i));
    }
    db_->insert(batch, DB::Columns::final_chain_transaction_hashes_by_blk_number, p, dev::rlp(new_data));
  }
  db_->commitWriteBatch(batch);
}
}  // namespace taraxa::storage::migration