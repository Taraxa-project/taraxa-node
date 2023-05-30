#pragma once
#include <libdevcore/Common.h>

#include "final_chain/final_chain.hpp"
#include "storage/migration/migration_base.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::storage::migration {
class TransactionHashes : public migration::Base {
 public:
  TransactionHashes(std::shared_ptr<DbStorage> db);
  std::string id() override;
  uint32_t dbVersion() override;
  void migrate(logger::Logger& log) override;
};
}  // namespace taraxa::storage::migration