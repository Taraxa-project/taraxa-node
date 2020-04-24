#ifndef TARAXA_NODE_ETH_ETH_SERVICE_HPP_
#define TARAXA_NODE_ETH_ETH_SERVICE_HPP_

#include <libethereum/BlockChain.h>
#include <libethereum/ClientBase.h>
#include <libweb3jsonrpc/AccountHolder.h>

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "database_adapter.hpp"

namespace taraxa {
class FullNode;
}
namespace taraxa::eth::eth {
class Eth;
}

namespace taraxa::eth::eth_service {
using database_adapter::DatabaseAdapter;
using dev::Address;
using dev::Addresses;
using dev::bytes;
using dev::h256;
using dev::h256s;
using dev::OverlayDB;
using dev::Secret;
using dev::u256;
using dev::WithExisting;
using dev::db::DatabaseFace;
using dev::eth::AccountHolder;
using dev::eth::Block;
using dev::eth::BlockChain;
using dev::eth::BlockDetails;
using dev::eth::BlockHeader;
using dev::eth::BlockNumber;
using dev::eth::ChainParams;
using dev::eth::ClientBase;
using dev::eth::ExecutionResult;
using dev::eth::FudgeFactor;
using dev::eth::GasEstimationCallback;
using dev::eth::ImportResult;
using dev::eth::LatestBlock;
using dev::eth::LocalisedLogEntries;
using dev::eth::LocalisedTransaction;
using dev::eth::LocalisedTransactionReceipt;
using dev::eth::LogFilter;
using dev::eth::Nonce;
using dev::eth::PendingBlock;
using dev::eth::ProgressCallback;
using dev::eth::Reaping;
using dev::eth::SealEngineFace;
using dev::eth::State;
using dev::eth::SyncStatus;
using dev::eth::Transaction;
using dev::eth::TransactionHashes;
using dev::eth::TransactionReceipt;
using dev::eth::TransactionReceipts;
using dev::eth::Transactions;
using dev::eth::TransactionSkeleton;
using dev::eth::UncleHashes;
using std::atomic;
using std::list;
using std::map;
using std::mutex;
using std::pair;
using std::shared_ptr;
using std::thread;
using std::tuple;
using std::unique_ptr;
using std::weak_ptr;
using std::chrono::milliseconds;

namespace fs = boost::filesystem;

static inline auto const err_not_applicable =
    std::runtime_error("Method not applicable");

class EthService : virtual ClientBase {
  friend class ::taraxa::eth::eth::Eth;

  weak_ptr<FullNode> node_;
  shared_ptr<DatabaseAdapter> db_adapter_;
  shared_ptr<DatabaseAdapter> extras_db_adapter_;
  BlockChain bc_;
  thread gc_thread_;
  atomic<bool> destructor_called_ = false;

 public:
  shared_ptr<AccountHolder> const current_node_account_holder;

  EthService(shared_ptr<FullNode> const& node,  //
             ChainParams const& chain_params,
             BlockChain::CacheConfig const& cache_config = {},
             milliseconds gc_trigger_backoff = milliseconds(100));

  ~EthService();

  using ClientBase::isKnownTransaction;

  SealEngineFace* sealEngine() const override { return bc().sealEngine(); }

  State getAccountsState() const;

  BlockHeader commitBlock(DbStorage::BatchPtr batch, Address const& author,
                          int64_t timestamp, Transactions const& transactions);

  void update_head() { bc_.update_head(); }

  template <typename... Arg>
  auto accountNonce(Arg const&... args) const {
    return countAt(args...);
  }

  BlockHeader head() const;

 private:
  h256 submitTransaction(TransactionSkeleton const& _t,
                         Secret const& _secret) override;
  h256 importTransaction(Transaction const& _t) override;
  ExecutionResult call(Address const& _from, u256 _value, Address _dest,
                       bytes const& _data, u256 _gas, u256 _gasPrice,
                       BlockNumber _blockNumber, FudgeFactor _ff) override;
  Transactions pending() const override;
  TransactionSkeleton populateTransactionWithDefaults(
      TransactionSkeleton const& _t) const override;
  Address author() const override;
  SyncStatus syncStatus() const override;

  ImportResult injectBlock(bytes const& _block) override {
    throw err_not_applicable;
  }
  bool isSyncing() const override { throw err_not_applicable; }
  bool isMajorSyncing() const override { throw err_not_applicable; }
  void setAuthor(Address const& _us) override { throw err_not_applicable; }
  tuple<h256, h256, h256> getWork() override { throw err_not_applicable; }
  void flushTransactions() override { throw err_not_applicable; }

 protected:
  BlockChain& bc() override;
  BlockChain const& bc() const override;
  Block block(h256 const& _h) const override;
  Block preSeal() const override;
  Block postSeal() const override;

  void prepareForTransaction() override { throw err_not_applicable; }
};

}  // namespace taraxa::eth::eth_service

#endif  // TARAXA_NODE_ETH_ETH_SERVICE_HPP_
