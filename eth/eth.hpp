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

class Eth : public virtual dev::rpc::Eth {
  shared_ptr<EthService> eth_service_;
  shared_ptr<AccountHolder> eth_accounts_;

 public:
  Eth(shared_ptr<EthService> eth_service,
      shared_ptr<AccountHolder> eth_accounts)
      : eth_service_(eth_service),
        eth_accounts_(eth_accounts),
        dev::rpc::Eth(*eth_service, *eth_accounts) {}

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
  bool eth_flush() override {
    BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_METHOD_NOT_FOUND));
  }
};

}  // namespace taraxa::eth::eth

#endif  // TARAXA_NODE_ETH_ETH_HPP_
