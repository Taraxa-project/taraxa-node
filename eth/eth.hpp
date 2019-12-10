#ifndef TARAXA_NODE_ETH_ETH_HPP_
#define TARAXA_NODE_ETH_ETH_HPP_

#include <libweb3jsonrpc/Eth.h>

#include "eth_service.hpp"

namespace taraxa::eth::eth {
using dev::eth::AccountHolder;
using eth_service::EthService;
using jsonrpc::Errors;
using jsonrpc::JsonRpcException;
using std::shared_ptr;
using std::string;

class Eth : public dev::rpc::Eth {
  shared_ptr<EthService> eth_service_;
  shared_ptr<AccountHolder> eth_accounts_;

 public:
  Eth(shared_ptr<EthService> eth_service,
      shared_ptr<AccountHolder> eth_accounts)
      : eth_service_(eth_service),
        eth_accounts_(eth_accounts),
        dev::rpc::Eth(*eth_service, *eth_accounts) {}

//  RPCModules implementedModules() const override;
//
//  // JSON-RPC
//  string eth_protocolVersion() override;
//  Json::Value eth_syncing() override;
//  string eth_coinbase() override;
//  string eth_gasPrice() override;
//  Json::Value eth_accounts() override;
//  string eth_blockNumber() override;
//  string eth_getBalance(const string& param1, const string& param2) override;
//  string eth_getStorageAt(const string& param1, const string& param2,
//                          const string& param3) override;
//  string eth_getTransactionCount(const string& param1,
//                                 const string& param2) override;
//  Json::Value eth_getBlockTransactionCountByHash(const string& param1) override;
//  Json::Value eth_getBlockTransactionCountByNumber(
//      const string& param1) override;
//  Json::Value eth_getUncleCountByBlockHash(const string& param1) override;
//  Json::Value eth_getUncleCountByBlockNumber(const string& param1) override;
//  string eth_getCode(const string& param1, const string& param2) override;
//  Json::Value eth_signTransaction(const Json::Value& param1) override;
//  string eth_sendTransaction(const Json::Value& param1) override;
//  string eth_sendRawTransaction(const string& param1) override;
//  string eth_getStorageRoot(const string& param1,
//                            const string& param2) override;
//  string eth_call(const Json::Value& param1, const string& param2) override;
//  string eth_estimateGas(const Json::Value& param1) override;
//  Json::Value eth_getBlockByHash(const string& param1, bool param2) override;
//  Json::Value eth_getBlockByNumber(const string& param1, bool param2) override;
//  Json::Value eth_getTransactionByHash(const string& param1) override;
//  Json::Value eth_getTransactionByBlockHashAndIndex(
//      const string& param1, const string& param2) override;
//  Json::Value eth_getTransactionByBlockNumberAndIndex(
//      const string& param1, const string& param2) override;
//  Json::Value eth_getTransactionReceipt(const string& param1) override;
//  Json::Value eth_pendingTransactions() override;
//  Json::Value eth_getUncleByBlockHashAndIndex(const string& param1,
//                                              const string& param2) override;
//  Json::Value eth_getUncleByBlockNumberAndIndex(const string& param1,
//                                                const string& param2) override;
//  string eth_newFilter(const Json::Value& param1) override;
//  string eth_newBlockFilter() override;
//  string eth_newPendingTransactionFilter() override;
//  bool eth_uninstallFilter(const string& param1) override;
//  Json::Value eth_getFilterChanges(const string& param1) override;
//  Json::Value eth_getFilterLogs(const string& param1) override;
//  Json::Value eth_getLogs(const Json::Value& param1) override;

  Json::Value eth_getWork() override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
  bool eth_submitWork(const string& param1, const string& param2,
                      const string& param3) override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
  bool eth_submitHashrate(const string& param1, const string& param2) override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
  bool eth_mining() override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
  string eth_hashrate() override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }

  // Aleth-specific
//  string eth_newFilterEx(const Json::Value& param1) override;
//  Json::Value eth_getFilterChangesEx(const string& param1) override;
//  Json::Value eth_getFilterLogsEx(const string& param1) override;
//  Json::Value eth_getLogsEx(const Json::Value& param1) override;
//  string eth_register(const string& param1) override;
//  bool eth_unregister(const string& param1) override;
//  Json::Value eth_fetchQueuedTransactions(const string& param1) override;
//  Json::Value eth_inspectTransaction(const string& param1) override;
//  bool eth_notePassword(const string& param1) override;
//  string eth_chainId() override;
  bool eth_flush() override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
};

}  // namespace taraxa::eth::eth

#endif  // TARAXA_NODE_ETH_ETH_HPP_
