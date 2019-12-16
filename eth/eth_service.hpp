#ifndef TARAXA_NODE_ETH_ETH_SERVICE_HPP_
#define TARAXA_NODE_ETH_ETH_SERVICE_HPP_

#include <libethereum/BlockChain.h>
#include <libethereum/ClientBase.h>

#include <mutex>

namespace taraxa {
class FullNode;
}

namespace taraxa::eth::eth_service {
using dev::Address;
using dev::Addresses;
using dev::bytes;
using dev::h256;
using dev::h256s;
using dev::OverlayDB;
using dev::Secret;
using dev::u256;
using dev::WithExisting;
using dev::eth::Block;
using dev::eth::BlockChain;
using dev::eth::BlockDetails;
using dev::eth::BlockHeader;
using dev::eth::BlockNumber;
using dev::eth::ChainParams;
using dev::eth::ExecutionResult;
using dev::eth::FudgeFactor;
using dev::eth::GasEstimationCallback;
using dev::eth::ImportResult;
using dev::eth::LocalisedLogEntries;
using dev::eth::LocalisedTransaction;
using dev::eth::LocalisedTransactionReceipt;
using dev::eth::LogFilter;
using dev::eth::ProgressCallback;
using dev::eth::Reaping;
using dev::eth::State;
using dev::eth::SyncStatus;
using dev::eth::Transaction;
using dev::eth::TransactionHashes;
using dev::eth::TransactionReceipt;
using dev::eth::TransactionReceipts;
using dev::eth::Transactions;
using dev::eth::TransactionSkeleton;
using dev::eth::UncleHashes;
using std::map;
using std::mutex;
using std::pair;
using std::tuple;
using std::weak_ptr;

namespace fs = boost::filesystem;

class EthService : public dev::eth::ClientBase {
  weak_ptr<FullNode> node_;
  BlockChain bc_;
  OverlayDB acc_state_db_;
  mutex append_block_mu_;

 public:
  EthService(weak_ptr<FullNode> const& node,  //
             ChainParams const& chain_params,
             fs::path const& db_base_path,  //
             WithExisting with_existing = WithExisting::Trust,
             ProgressCallback const& progress_cb = ProgressCallback());

  h256 submitTransaction(TransactionSkeleton const& _t,
                         Secret const& _secret) override;
  h256 importTransaction(Transaction const& _t) override;
  ExecutionResult call(Address const& _from, u256 _value, Address _dest,
                       bytes const& _data, u256 _gas, u256 _gasPrice,
                       BlockNumber _blockNumber, FudgeFactor _ff) override;
  Transactions pending() const override;
  TransactionSkeleton populateTransactionWithDefaults(
      TransactionSkeleton const& _t) const override;

  void appendBlock(Transactions const& transactions,
                   TransactionReceipts const& receipts,  //
                   h256 state_root,                      //
                   int64_t timestamp,                    //
                   Address author);

  Address author() const override;
  SyncStatus syncStatus() const override;

  ImportResult injectBlock(bytes const& _block) override { assert(false); }
  bool isSyncing() const override { assert(false); }
  bool isMajorSyncing() const override { assert(false); }
  void setAuthor(Address const& _us) override { assert(false); }
  tuple<h256, h256, h256> getWork() override { assert(false); }
  void flushTransactions() override { assert(false); }

 protected:
  BlockChain& bc() override;
  BlockChain const& bc() const override;
  Block block(h256 const& _h) const override;
  Block preSeal() const override;
  Block postSeal() const override;

  void prepareForTransaction() override { assert(false); }
};

}  // namespace taraxa::eth::eth_service

#endif  // TARAXA_NODE_ETH_ETH_SERVICE_HPP_
