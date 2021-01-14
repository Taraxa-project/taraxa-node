#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>

#include <iosfwd>
#include <memory>
#include <optional>

#include "EthFace.h"
#include "aleth/Common.h"
#include "aleth/CommonNet.h"
#include "final_chain.hpp"

namespace taraxa::net {

struct Eth : EthFace {
  using GasPricer = std::function<dev::u256()>;
  struct NodeAPI {
    virtual ~NodeAPI() {}
    virtual uint64_t chain_id() const = 0;
    virtual aleth::SyncStatus syncStatus() const = 0;
    virtual dev::Address const& address() const = 0;
    virtual dev::h256 importTransaction(aleth::TransactionSkeleton const& _t) = 0;
    virtual dev::h256 importTransaction(dev::bytes&& rlp) = 0;
  };
  struct FilterAPI {
    using FilterID = uint64_t;

    virtual ~FilterAPI() {}

    virtual std::optional<aleth::LogFilter> getLogFilter(FilterID id) const = 0;
    virtual FilterID newBlockFilter() = 0;
    virtual FilterID newPendingTransactionFilter() = 0;
    virtual FilterID newLogFilter(aleth::LogFilter const& _filter) = 0;
    virtual bool uninstallFilter(FilterID id) = 0;

    struct Consumer {
      std::function<void(aleth::LocalisedLogEntries const&)> onLogFilterChanges;
      std::function<void(dev::h256s const&)> onPendingTransactionFilterChanges;
      std::function<void(dev::h256s const&)> onBlockFilterChanges;
    };
    virtual void poll(FilterID id, Consumer const& consumer) = 0;
  };
  struct StateAPI {
    virtual ~StateAPI() {}

    virtual dev::bytes call(aleth::BlockNumber _blockNumber, aleth::TransactionSkeleton const& trx) const = 0;
    virtual uint64_t estimateGas(aleth::BlockNumber _blockNumber, aleth::TransactionSkeleton const& trx) const = 0;
    virtual dev::u256 balanceAt(dev::Address _a, aleth::BlockNumber _block) const = 0;
    virtual uint64_t nonceAt(dev::Address _a, aleth::BlockNumber _block) const = 0;
    virtual dev::bytes codeAt(dev::Address _a, aleth::BlockNumber _block) const = 0;
    virtual dev::h256 stateRootAt(dev::Address _a, aleth::BlockNumber _block) const = 0;
    virtual dev::u256 stateAt(dev::Address _a, dev::u256 _l, aleth::BlockNumber _block) const = 0;
  };
  struct PendingBlock {
    virtual ~PendingBlock() {}

    virtual aleth::BlockDetails details() const = 0;
    virtual uint64_t transactionsCount() const = 0;
    virtual uint64_t transactionsCount(dev::Address const& from) const = 0;
    virtual Transactions transactions() const = 0;
    virtual std::optional<Transaction> transaction(unsigned index) const = 0;
    virtual dev::h256s transactionHashes() const = 0;
    virtual aleth::BlockHeader header() const = 0;
  };

 private:
  std::shared_ptr<NodeAPI> node_api;
  std::shared_ptr<FilterAPI> filter_api;
  std::shared_ptr<StateAPI> state_api;
  std::shared_ptr<PendingBlock> pending_block;
  std::shared_ptr<FinalChain> chain_db;
  GasPricer gas_pricer;

 public:
  Eth(decltype(node_api) node_api, decltype(filter_api) filter_api, decltype(state_api) state_api,
      decltype(pending_block) pending_block, decltype(chain_db) chain_db, decltype(gas_pricer) gas_pricer)
      : node_api(std::move(node_api)),
        filter_api(std::move(filter_api)),
        state_api(std::move(state_api)),
        pending_block(std::move(pending_block)),
        chain_db(std::move(chain_db)),
        gas_pricer(std::move(gas_pricer)){};

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"eth", "1.0"}}; }

  virtual std::string eth_protocolVersion() override;
  virtual std::string eth_coinbase() override;
  virtual std::string eth_gasPrice() override;
  virtual Json::Value eth_accounts() override;
  virtual std::string eth_blockNumber() override;
  virtual std::string eth_getBalance(std::string const& _address, std::string const& _blockNumber) override;
  virtual std::string eth_getStorageAt(std::string const& _address, std::string const& _position,
                                       std::string const& _blockNumber) override;
  virtual std::string eth_getStorageRoot(std::string const& _address, std::string const& _blockNumber) override;
  virtual std::string eth_getTransactionCount(std::string const& _address, std::string const& _blockNumber) override;
  virtual Json::Value eth_pendingTransactions() override;
  virtual Json::Value eth_getBlockTransactionCountByHash(std::string const& _blockHash) override;
  virtual Json::Value eth_getBlockTransactionCountByNumber(std::string const& _blockNumber) override;
  virtual Json::Value eth_getUncleCountByBlockHash(std::string const& _blockHash) override;
  virtual Json::Value eth_getUncleCountByBlockNumber(std::string const& _blockNumber) override;
  virtual std::string eth_getCode(std::string const& _address, std::string const& _blockNumber) override;
  virtual std::string eth_sendTransaction(Json::Value const& _json) override;
  virtual std::string eth_call(Json::Value const& _json, std::string const& _blockNumber) override;
  virtual std::string eth_estimateGas(Json::Value const& _json) override;
  virtual Json::Value eth_getBlockByHash(std::string const& _blockHash, bool _includeTransactions) override;
  virtual Json::Value eth_getBlockByNumber(std::string const& _blockNumber, bool _includeTransactions) override;
  virtual Json::Value eth_getTransactionByHash(std::string const& _transactionHash) override;
  virtual Json::Value eth_getTransactionByBlockHashAndIndex(std::string const& _blockHash,
                                                            std::string const& _transactionIndex) override;
  virtual Json::Value eth_getTransactionByBlockNumberAndIndex(std::string const& _blockNumber,
                                                              std::string const& _transactionIndex) override;
  virtual Json::Value eth_getTransactionReceipt(std::string const& _transactionHash) override;
  virtual Json::Value eth_getUncleByBlockHashAndIndex(std::string const& _blockHash,
                                                      std::string const& _uncleIndex) override;
  virtual Json::Value eth_getUncleByBlockNumberAndIndex(std::string const& _blockNumber,
                                                        std::string const& _uncleIndex) override;
  virtual std::string eth_newFilter(Json::Value const& _json) override;
  virtual std::string eth_newBlockFilter() override;
  virtual std::string eth_newPendingTransactionFilter() override;
  virtual bool eth_uninstallFilter(std::string const& _filterId) override;
  virtual Json::Value eth_getFilterChanges(std::string const& _filterId) override;
  virtual Json::Value eth_getFilterLogs(std::string const& _filterId) override;
  virtual Json::Value eth_getLogs(Json::Value const& _json) override;
  virtual std::string eth_sendRawTransaction(std::string const& _rlp) override;
  virtual Json::Value eth_syncing() override;
  virtual Json::Value eth_chainId() override;

 private:
  uint64_t transaction_count(std::optional<aleth::BlockNumber> n, dev::Address const& addr);

  void populateTransactionWithDefaults(aleth::TransactionSkeleton& _t,
                                       std::optional<aleth::BlockNumber> blk_n = std::nullopt);
  std::optional<aleth::BlockNumber> jsToBlockNumber(std::string const& _js,
                                                    std::optional<aleth::BlockNumber> latest_block = std::nullopt);
  aleth::BlockNumber jsToBlockNumberPendingAsLatest(std::string const& js,
                                                    std::optional<aleth::BlockNumber> latest_block = std::nullopt);
  aleth::LogFilter toLogFilter(Json::Value const& _json);
};

}  // namespace taraxa::net
