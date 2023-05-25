#pragma once
#include <libdevcore/Common.h>

#include "common/thread_pool.hpp"
#include "pbft/period_data.hpp"
#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {
class PeriodDataReorder : public migration::Base {
 public:
  PeriodDataReorder(std::shared_ptr<DbStorage> db);
  std::string id() override;
  uint32_t dbVersion() override;
  void migrate(logger::Logger& log) override;
};
}  // namespace taraxa::storage::migration