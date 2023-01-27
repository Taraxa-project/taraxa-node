/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_TARAXA_NET_TESTFACE_H_
#define JSONRPC_CPP_STUB_TARAXA_NET_TESTFACE_H_

#include <libweb3jsonrpc/ModularServer.h>

namespace taraxa {
namespace net {
class TestFace : public ServerInterface<TestFace> {
 public:
  TestFace() {
    this->bindAndAddMethod(jsonrpc::Procedure("get_sortition_change", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                                              "param1", jsonrpc::JSON_OBJECT, NULL),
                           &taraxa::net::TestFace::get_sortition_changeI);
    this->bindAndAddMethod(jsonrpc::Procedure("send_coin_transaction", jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT, NULL),
                           &taraxa::net::TestFace::send_coin_transactionI);
    this->bindAndAddMethod(jsonrpc::Procedure("send_coin_transactions", jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT, NULL),
                           &taraxa::net::TestFace::send_coin_transactionsI);
    this->bindAndAddMethod(jsonrpc::Procedure("tps_test", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param1",
                                              jsonrpc::JSON_OBJECT, NULL),
                           &taraxa::net::TestFace::tps_testI);

    this->bindAndAddMethod(
        jsonrpc::Procedure("get_account_address", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TestFace::get_account_addressI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_peer_count", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TestFace::get_peer_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_node_status", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TestFace::get_node_statusI);
    this->bindAndAddMethod(jsonrpc::Procedure("get_all_nodes", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
                           &taraxa::net::TestFace::get_all_nodesI);
  }

  inline virtual void get_sortition_changeI(const Json::Value &request, Json::Value &response) {
    response = this->get_sortition_change(request[0u]);
  }
  inline virtual void send_coin_transactionI(const Json::Value &request, Json::Value &response) {
    response = this->send_coin_transaction(request[0u]);
  }
  inline virtual void send_coin_transactionsI(const Json::Value &request, Json::Value &response) {
    response = this->send_coin_transactions(request[0u]);
  }
  inline virtual void tps_testI(const Json::Value &request, Json::Value &response) {
    response = this->tps_test(request[0u]);
  }

  inline virtual void get_account_addressI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->get_account_address();
  }
  inline virtual void get_peer_countI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->get_peer_count();
  }
  inline virtual void get_node_statusI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->get_node_status();
  }
  inline virtual void get_all_nodesI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->get_all_nodes();
  }
  virtual Json::Value get_sortition_change(const Json::Value &param1) = 0;
  virtual Json::Value send_coin_transaction(const Json::Value &param1) = 0;
  virtual Json::Value send_coin_transactions(const Json::Value &param1) = 0;
  virtual Json::Value tps_test(const Json::Value &param1) = 0;
  virtual Json::Value get_account_address() = 0;
  virtual Json::Value get_peer_count() = 0;
  virtual Json::Value get_node_status() = 0;
  virtual Json::Value get_all_nodes() = 0;
};

}  // namespace net
}  // namespace taraxa
#endif  // JSONRPC_CPP_STUB_TARAXA_NET_TESTFACE_H_
