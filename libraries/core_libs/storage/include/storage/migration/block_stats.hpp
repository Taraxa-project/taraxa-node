#pragma once
#include <libdevcore/Common.h>

#include <config/config.hpp>

#include "storage/migration/migration_base.hpp"

namespace taraxa::storage::migration {

class BlockStats : public migration::Base {
 public:
  BlockStats(std::shared_ptr<DbStorage> db, const FullNodeConfig& config);
  std::string id() override;
  uint32_t dbVersion() override;

 protected:
  void migrate(logger::Logger& log) override;

 private:
  FullNodeConfig kConfig;
};

}  // namespace taraxa::storage::migration