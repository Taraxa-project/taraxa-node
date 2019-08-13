#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>
#include <iosfwd>
#include <memory>
#include <optional>
#include "../full_node.hpp"
#include "SessionManager.h"
#include "TaraxaFace.h"

namespace dev {

namespace rpc {

inline taraxa::dag_blk_num_t to_blk_num(std::string const& str) {
  assert(str.find("0x") == 0);
  return stoull(str, 0, 16);
}

inline taraxa::trx_num_t to_trx_num(std::string const& str) {
  assert(str.find("0x") == 0);
  auto const& num = stoull(str, 0, 16);
  assert(num <= std::numeric_limits<taraxa::trx_num_t>::max());
  return taraxa::trx_num_t(num);
}

// As per
// https://github.com/ethereum/wiki/wiki/JSON-RPC#the-default-block-parameter
struct BlockNumber {
  enum class Kind { earliest, latest, pending, specific };

  static BlockNumber const earliest, latest, pending;

  Kind const kind;
  std::optional<taraxa::dag_blk_num_t> const block_number;

  static BlockNumber from(std::string const& str) {
    if ("earliest" == str) return earliest;
    if ("latest" == str) return latest;
    if ("pending" == str) return pending;
    return from(to_blk_num(str));
  }

  static BlockNumber from(taraxa::dag_blk_num_t const& val) {
    return val == 0 ? earliest : BlockNumber(Kind::specific, val);
  }

 private:
  BlockNumber(decltype(kind)& kind,
              decltype(block_number)& block_number = std::nullopt)
      : kind(kind), block_number(block_number) {}
};
inline BlockNumber const BlockNumber::earliest(Kind::earliest, 0);
inline BlockNumber const BlockNumber::latest(Kind::latest);
inline BlockNumber const BlockNumber::pending(Kind::pending);

/**
 * @brief JSON-RPC api implementation
 */
class Taraxa : public dev::rpc::TaraxaFace {
 public:
  Taraxa(std::shared_ptr<taraxa::FullNode>& _full_node);

  virtual RPCModules implementedModules() const override {
    return RPCModules{RPCModule{"taraxa", "1.0"}};
  }

  virtual std::string taraxa_protocolVersion() override;
  virtual std::string taraxa_hashrate() override;
  virtual std::string taraxa_coinbase() override;
  virtual bool taraxa_mining() override;
  virtual std::string taraxa_gasPrice() override;
  virtual Json::Value taraxa_accounts() override;
  virtual std::string taraxa_blockNumber() override;
  virtual std::string taraxa_getBalance(
      std::string const& _address, std::string const& _blockNumber) override;
  virtual std::string taraxa_getStorageAt(
      std::string const& _address, std::string const& _position,
      std::string const& _blockNumber) override;
  virtual std::string taraxa_getStorageRoot(
      std::string const& _address, std::string const& _blockNumber) override;
  virtual std::string taraxa_getTransactionCount(
      std::string const& _address, std::string const& _blockNumber) override;
  virtual Json::Value taraxa_pendingTransactions() override;
  virtual Json::Value taraxa_getBlockTransactionCountByHash(
      std::string const& _blockHash) override;
  virtual Json::Value taraxa_getBlockTransactionCountByNumber(
      std::string const& _blockNumber) override;
  virtual Json::Value taraxa_getUncleCountByBlockHash(
      std::string const& _blockHash) override;
  virtual Json::Value taraxa_getUncleCountByBlockNumber(
      std::string const& _blockNumber) override;
  virtual std::string taraxa_getCode(std::string const& _address,
                                     std::string const& _blockNumber) override;
  virtual std::string taraxa_sendTransaction(Json::Value const& _json) override;
  virtual std::string taraxa_call(Json::Value const& _json,
                                  std::string const& _blockNumber) override;
  virtual std::string taraxa_estimateGas(Json::Value const& _json) override;
  virtual bool taraxa_flush() override;
  virtual Json::Value taraxa_getBlockByHash(std::string const& _blockHash,
                                            bool _includeTransactions) override;
  virtual Json::Value taraxa_getBlockByNumber(
      std::string const& _blockNumber, bool _includeTransactions) override;
  virtual Json::Value taraxa_getTransactionByHash(
      std::string const& _transactionHash) override;
  virtual Json::Value taraxa_getTransactionByBlockHashAndIndex(
      std::string const& _blockHash,
      std::string const& _transactionIndex) override;
  virtual Json::Value taraxa_getTransactionByBlockNumberAndIndex(
      std::string const& _blockNumber,
      std::string const& _transactionIndex) override;
  virtual Json::Value taraxa_getTransactionReceipt(
      std::string const& _transactionHash) override;
  virtual Json::Value taraxa_getUncleByBlockHashAndIndex(
      std::string const& _blockHash, std::string const& _uncleIndex) override;
  virtual Json::Value taraxa_getUncleByBlockNumberAndIndex(
      std::string const& _blockNumber, std::string const& _uncleIndex) override;
  virtual std::string taraxa_newFilter(Json::Value const& _json) override;
  virtual std::string taraxa_newFilterEx(Json::Value const& _json) override;
  virtual std::string taraxa_newBlockFilter() override;
  virtual std::string taraxa_newPendingTransactionFilter() override;
  virtual bool taraxa_uninstallFilter(std::string const& _filterId) override;
  virtual Json::Value taraxa_getFilterChanges(
      std::string const& _filterId) override;
  virtual Json::Value taraxa_getFilterChangesEx(
      std::string const& _filterId) override;
  virtual Json::Value taraxa_getFilterLogs(
      std::string const& _filterId) override;
  virtual Json::Value taraxa_getFilterLogsEx(
      std::string const& _filterId) override;
  virtual Json::Value taraxa_getLogs(Json::Value const& _json) override;
  virtual Json::Value taraxa_getLogsEx(Json::Value const& _json) override;
  virtual Json::Value taraxa_getWork() override;
  virtual bool taraxa_submitWork(std::string const& _nonce, std::string const&,
                                 std::string const& _mixHash) override;
  virtual bool taraxa_submitHashrate(std::string const& _hashes,
                                     std::string const& _id) override;
  virtual std::string taraxa_register(std::string const& _address) override;
  virtual bool taraxa_unregister(std::string const& _accountId) override;
  virtual Json::Value taraxa_fetchQueuedTransactions(
      std::string const& _accountId) override;
  virtual Json::Value taraxa_signTransaction(
      Json::Value const& _transaction) override;
  virtual Json::Value taraxa_inspectTransaction(
      std::string const& _rlp) override;
  virtual std::string taraxa_sendRawTransaction(
      std::string const& _rlp) override;
  virtual bool taraxa_notePassword(std::string const&) override {
    return false;
  }
  virtual Json::Value taraxa_syncing() override;
  virtual std::string taraxa_chainId() override;

 protected:
  std::weak_ptr<taraxa::FullNode> full_node_;

 private:
  using NodePtr = decltype(full_node_.lock());

  NodePtr tryGetNode();
  static std::optional<taraxa::StateRegistry::Snapshot> getSnapshot(
      NodePtr const&,  //
      BlockNumber const&);
  static std::shared_ptr<taraxa::StateRegistry::State> getState(
      NodePtr const&,  //
      BlockNumber const&);
  static Json::Value blockToJson(
      NodePtr const&,
      taraxa::blk_hash_t const&,                              //
      std::optional<taraxa::StateRegistry::Snapshot> const&,  //
      bool include_trx);
};

}  // namespace rpc
}  // namespace dev
