#pragma once
#include <libdevcore/Common.h>

#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {
class TransactionReceiptsByPeriod : public migration::Base {
 public:
  TransactionReceiptsByPeriod(std::shared_ptr<DbStorage> db);
  std::string id() override;
  uint32_t dbVersion() override;

 protected:
  void migrate(logger::Logger& logger) override;
};
}  // namespace taraxa::storage::migration