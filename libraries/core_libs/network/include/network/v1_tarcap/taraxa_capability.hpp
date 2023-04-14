#pragma once

#include "network/tarcap/taraxa_capability.hpp"

// namespace taraxa {
// class DbStorage;
// class PbftManager;
// class PbftChain;
// class VoteManager;
// class DagManager;
// class TransactionManager;
// enum class TransactionStatus;
// }  // namespace taraxa

namespace taraxa::network::v1_tarcap {

// class PacketsHandler;
// class PbftSyncingState;
// class TaraxaPeer;

class TaraxaCapability : public taraxa::network::tarcap::TaraxaCapability {
 public:
  TaraxaCapability(std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const FullNodeConfig &conf,
                   unsigned version, const std::string &log_channel);

  virtual ~TaraxaCapability() = default;
  TaraxaCapability(const TaraxaCapability &ro) = delete;
  TaraxaCapability &operator=(const TaraxaCapability &ro) = delete;
  TaraxaCapability(TaraxaCapability &&ro) = delete;
  TaraxaCapability &operator=(TaraxaCapability &&ro) = delete;
  // TODO: do we need this ???
  //  static std::shared_ptr<TaraxaCapability> make(
  //      std::weak_ptr<dev::p2p::Host> host, const dev::KeyPair &key, const FullNodeConfig &conf, const h256
  //      &genesis_hash, unsigned version, std::shared_ptr<DbStorage> db = {}, std::shared_ptr<PbftManager> pbft_mgr =
  //      {}, std::shared_ptr<PbftChain> pbft_chain = {}, std::shared_ptr<VoteManager> vote_mgr = {},
  //      std::shared_ptr<DagManager> dag_mgr = {}, std::shared_ptr<TransactionManager> trx_mgr = {});
  //
  //  // Init capability. Register packet handlers and periodic events
  //  virtual void init(const h256 &genesis_hash, std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
  //                    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
  //                    std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<TransactionManager> trx_mgr,
  //                    const dev::Address &node_addr);

 protected:
  virtual void registerPacketHandlers(
      const h256 &genesis_hash, const std::shared_ptr<tarcap::TimePeriodPacketsStats> &packets_stats,
      const std::shared_ptr<DbStorage> &db, const std::shared_ptr<PbftManager> &pbft_mgr,
      const std::shared_ptr<PbftChain> &pbft_chain, const std::shared_ptr<VoteManager> &vote_mgr,
      const std::shared_ptr<DagManager> &dag_mgr, const std::shared_ptr<TransactionManager> &trx_mgr,
      const addr_t &node_addr) override;

 private:
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::v1_tarcap
