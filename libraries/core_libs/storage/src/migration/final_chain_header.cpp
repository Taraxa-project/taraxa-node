#include "storage/migration/final_chain_header.hpp"

#include "common/thread_pool.hpp"
#include "final_chain/data.hpp"

namespace taraxa::storage::migration {

FinalChainHeader::FinalChainHeader(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string FinalChainHeader::id() { return "FinalChainHeader"; }

uint32_t FinalChainHeader::dbVersion() { return 1; }

struct OldHeader : final_chain::BlockHeaderData {
  Address author;
  EthBlockNumber number = 0;
  uint64_t gas_limit = 0;
  uint64_t timestamp = 0;
  bytes extra_data;

  RLP_FIELDS_DEFINE_INPLACE(hash, parent_hash, author, state_root, transactions_root, receipts_root, log_bloom, number,
                            gas_limit, gas_used, timestamp, total_reward, extra_data)
};

void FinalChainHeader::migrate(logger::Logger& log) {
  auto it = db_->getColumnIterator(DbStorage::Columns::final_chain_blk_by_number);
  it->SeekToFirst();
  if (!it->Valid()) {
    LOG(log) << "No blocks to migrate";
    return;
  }

  uint64_t start_period, end_period;
  memcpy(&start_period, it->key().data(), sizeof(uint64_t));

  it->SeekToLast();
  if (!it->Valid()) {
    it->Prev();
  }
  memcpy(&end_period, it->key().data(), sizeof(uint64_t));
  util::ThreadPool executor{std::thread::hardware_concurrency()};

  const auto diff = (end_period - start_period) ? (end_period - start_period) : 1;
  uint64_t curr_progress = 0;
  std::cout << "Migrating " << diff << " blocks" << std::endl;
  std::cout << "Start period: " << start_period << ", end period: " << end_period << std::endl;
  // Get and save data in new format for all blocks
  it->SeekToFirst();
  for (; it->Valid(); it->Next()) {
    uint64_t period;
    memcpy(&period, it->key().data(), sizeof(uint64_t));
    std::string raw = it->value().ToString();
    executor.post([this, period, raw = std::move(raw), &copied_col]() {
      auto header = std::make_shared<OldHeader>();
      header->rlp(dev::RLP(raw));
      auto newBytes = header->serializeForDB();
      db_->insert(copied_col.get(), period, newBytes);
    });
  }
}
}  // namespace taraxa::storage::migration