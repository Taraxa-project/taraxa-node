#pragma once
#include <libdevcore/Common.h>

#include "final_chain/final_chain.hpp"
#include "storage/migration/migration_base.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::storage::migration {
class TransactionHashes : public migration::Base {
 public:
  TransactionHashes(std::shared_ptr<DbStorage> db) : migration::Base(db) {}
  std::string id() override { return "TransactionHashes"; }
  void migrate() override;
};
}  // namespace taraxa::storage::migration