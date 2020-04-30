#ifndef TARAXA_NODE_ETH_ETH_SERVICE_HPP_
#define TARAXA_NODE_ETH_ETH_SERVICE_HPP_

#include <libethereum/BlockChain.h>
#include <libethereum/ClientBase.h>

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
using dev::eth::BlockChain;
using dev::eth::BlockDetails;
using dev::eth::BlockHeader;
using dev::eth::BlockNumber;
using dev::eth::FudgeFactor;
using dev::eth::LatestBlock;
using dev::eth::LocalisedLogEntries;
using dev::eth::LocalisedTransaction;
using dev::eth::LocalisedTransactionReceipt;
using dev::eth::LogFilter;
using dev::eth::Nonce;
using dev::eth::PendingBlock;
using dev::eth::Reaping;
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

class EthService : public virtual dev::eth::ClientBase {
  weak_ptr<FullNode> node_;
  shared_ptr<DatabaseAdapter> db_adapter_;
  shared_ptr<DatabaseAdapter> extras_db_adapter_;
  BlockChain bc_;
  thread bc_gc_thread_;
  atomic<bool> destructor_called_ = false;

 public:
  EthService(shared_ptr<FullNode> const& node,  //
             BlockChain::CacheConfig const& cache_config = {},
             milliseconds gc_trigger_backoff = milliseconds(100));

  ~EthService();

  BlockHeader commitBlock(DbStorage::BatchPtr batch, Address const& author,
                          int64_t timestamp, Transactions const& transactions);

  void update_head() { bc_.update_head(); }

  BlockHeader head() const;

  Transaction sign(TransactionSkeleton const& _t) override;
  h256 importTransaction(Transaction const& _t) override;
  std::string call(BlockNumber _blockNumber,
                   TransactionSkeleton const& trx) const override;
  Transactions pendingTransactions() const override;
  void populateTransactionWithDefaults(TransactionSkeleton& _t) const override;
  Address author() const override;
  SyncStatus syncStatus() const override;

 protected:
  BlockChain& bc() override;
  BlockChain const& bc() const override;
};

}  // namespace taraxa::eth::eth_service

#endif  // TARAXA_NODE_ETH_ETH_SERVICE_HPP_
