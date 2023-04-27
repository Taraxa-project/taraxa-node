#pragma once

#include "network/tarcap/taraxa_capability_base.hpp"

// NoteL latest taraxa capability does not use any special namespace, other version use for example
// taraxa::network::tarcap::v1
namespace taraxa::network::tarcap {

class TaraxaCapability : public taraxa::network::tarcap::TaraxaCapabilityBase {
 public:
  using TaraxaCapabilityBase::TaraxaCapabilityBase;

  virtual ~TaraxaCapability() = default;
  TaraxaCapability(const TaraxaCapability &ro) = delete;
  TaraxaCapability &operator=(const TaraxaCapability &ro) = delete;
  TaraxaCapability(TaraxaCapability &&ro) = delete;
  TaraxaCapability &operator=(TaraxaCapability &&ro) = delete;

 protected:
  void registerPacketHandlers(const h256 &genesis_hash,
                              const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats,
                              const std::shared_ptr<DbStorage> &db, const std::shared_ptr<PbftManager> &pbft_mgr,
                              const std::shared_ptr<PbftChain> &pbft_chain,
                              const std::shared_ptr<VoteManager> &vote_mgr, const std::shared_ptr<DagManager> &dag_mgr,
                              const std::shared_ptr<TransactionManager> &trx_mgr, const addr_t &node_addr) override;

 private:
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
