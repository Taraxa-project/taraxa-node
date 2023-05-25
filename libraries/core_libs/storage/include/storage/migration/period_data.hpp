#pragma once
#include <libdevcore/Common.h>

#include "final_chain/final_chain.hpp"
#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {
class PeriodData : public migration::Base {
 public:
  PeriodData(std::shared_ptr<DbStorage> db);
  std::string id() override;
  uint32_t dbVersion() override;
  void migrate() override;
};
}  // namespace taraxa::storage::migration