#include "storage/migration/period_data.hpp"

namespace taraxa::storage::migration {

PeriodData::PeriodData(std::shared_ptr<DbStorage> db) : migration::Base(db) {}

std::string PeriodData::id() { return "PeriodData"; }

uint32_t PeriodData::dbVersion() { return 1; }

void PeriodData::migrate() {
  auto it = db_->getColumnIterator(DB::Columns::period_data);

  // Get and save data in new format for all blocks
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const auto period_data_old_rlp = dev::RLP(it->value().ToString());
    assert(period_data_old_rlp.itemCount() == 4);

    ::taraxa::PeriodData period_data;
    period_data.pbft_blk = std::make_shared<PbftBlock>(period_data_old_rlp[0]);
    for (auto const vote_rlp : period_data_old_rlp[1]) {
      period_data.previous_block_cert_votes.emplace_back(std::make_shared<Vote>(vote_rlp));
    }

    for (auto const dag_block_rlp : period_data_old_rlp[2]) {
      period_data.dag_blocks.emplace_back(dag_block_rlp);
    }

    for (auto const trx_rlp : period_data_old_rlp[3]) {
      period_data.transactions.emplace_back(std::make_shared<Transaction>(trx_rlp));
    }

    const auto period_data_new_rlp = period_data.rlp();
    db_->insert(batch_, DB::Columns::period_data, it->key(), period_data_new_rlp);
  }
}
}  // namespace taraxa::storage::migration