/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

// TODO re-generate

#ifndef TARAXA_NODE_NET_TEST_FACE_H_
#define TARAXA_NODE_NET_TEST_FACE_H_

#include <libweb3jsonrpc/ModularServer.h>

namespace taraxa::net {

class TestFace : public ServerInterface<TestFace> {
 public:
  TestFace() {
    this->bindAndAddMethod(
        jsonrpc::Procedure("insert_dag_block", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::insert_dag_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_blockI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_block_epfriend",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_dag_block_epfriendI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("send_coin_transaction", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::send_coin_transactionI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("create_test_coin_transactions",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::create_test_coin_transactionsI);
    this->bindAndAddMethod(jsonrpc::Procedure("get_num_proposed_blocks",
                                              jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, NULL),
                           &TestFace::get_num_proposed_blocksI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_account_address", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_account_addressI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_account_balance", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_account_balanceI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_peer_count", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_peer_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_node_status", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_node_statusI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_node_count", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_node_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_all_peers", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_all_peersI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_all_nodes", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_all_nodesI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("should_speak", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::should_speakI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("place_vote", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::place_voteI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_votes", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_votesI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("draw_graph", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::draw_graphI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_transaction_count", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_transaction_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_executed_blk_count",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_executed_blk_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_executed_trx_count",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                           "param1", jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_executed_trx_countI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_dag_size", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_dag_sizeI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_pbft_chain_size", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &TestFace::get_pbft_chain_sizeI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("get_pbft_chain_blocks", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT,
                           NULL),
        &TestFace::get_pbft_chain_blocksI);
  }
  inline virtual void insert_dag_blockI(const Json::Value &request,
                                        Json::Value &response) {
    response = this->insert_dag_block(request[0u]);
  }
  inline virtual void get_dag_blockI(const Json::Value &request,
                                     Json::Value &response) {
    response = this->get_dag_block(request[0u]);
  }
  inline virtual void get_dag_block_epfriendI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_dag_block_epfriend(request[0u]);
  }
  inline virtual void send_coin_transactionI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->send_coin_transaction(request[0u]);
  }
  inline virtual void create_test_coin_transactionsI(const Json::Value &request,
                                                     Json::Value &response) {
    response = this->create_test_coin_transactions(request[0u]);
  }
  inline virtual void get_num_proposed_blocksI(const Json::Value &request,
                                               Json::Value &response) {
    response = this->get_num_proposed_blocks();
  }
  inline virtual void get_account_addressI(const Json::Value &request,
                                           Json::Value &response) {
    response = this->get_account_address();
  }
  inline virtual void get_account_balanceI(const Json::Value &request,
                                           Json::Value &response) {
    response = this->get_account_balance(request[0u]);
  }
  inline virtual void get_peer_countI(const Json::Value &request,
                                      Json::Value &response) {
    response = this->get_peer_count();
  }
  inline virtual void get_node_statusI(const Json::Value &request,
                                      Json::Value &response) {
    response = this->get_node_status();
  }
  inline virtual void get_node_countI(const Json::Value &request,
                                      Json::Value &response) {
    response = this->get_node_count();
  }
  inline virtual void get_all_peersI(const Json::Value &request,
                                     Json::Value &response) {
    response = this->get_all_peers();
  }
  inline virtual void get_all_nodesI(const Json::Value &request,
                                     Json::Value &response) {
    response = this->get_all_nodes();
  }
  inline virtual void should_speakI(const Json::Value &request,
                                    Json::Value &response) {
    response = this->should_speak(request[0u]);
  }
  inline virtual void place_voteI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->place_vote(request[0u]);
  }
  inline virtual void get_votesI(const Json::Value &request,
                                 Json::Value &response) {
    response = this->get_votes(request[0u]);
  }
  inline virtual void draw_graphI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->draw_graph(request[0u]);
  }
  inline virtual void get_transaction_countI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->get_transaction_count(request[0u]);
  }
  inline virtual void get_executed_trx_countI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_executed_trx_count(request[0u]);
  }
  inline virtual void get_executed_blk_countI(const Json::Value &request,
                                              Json::Value &response) {
    response = this->get_executed_blk_count(request[0u]);
  }
  inline virtual void get_dag_sizeI(const Json::Value &request,
                                    Json::Value &response) {
    response = this->get_dag_size(request[0u]);
  }
  inline virtual void get_pbft_chain_sizeI(const Json::Value &request,
                                           Json::Value &response) {
    response = this->get_pbft_chain_size();
  }
  inline virtual void get_pbft_chain_blocksI(const Json::Value &request,
                                             Json::Value &response) {
    response = this->get_pbft_chain_blocks(request[0u]);
  }
  virtual Json::Value insert_dag_block(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block(const Json::Value &param1) = 0;
  virtual Json::Value get_dag_block_epfriend(const Json::Value &param1) = 0;
  virtual Json::Value send_coin_transaction(const Json::Value &param1) = 0;
  virtual Json::Value create_test_coin_transactions(
      const Json::Value &param1) = 0;
  virtual Json::Value get_num_proposed_blocks() = 0;
  virtual Json::Value get_account_address() = 0;
  virtual Json::Value get_account_balance(const Json::Value &param1) = 0;
  virtual Json::Value get_peer_count() = 0;
  virtual Json::Value get_node_status() = 0;
  virtual Json::Value get_node_count() = 0;
  virtual Json::Value get_all_peers() = 0;
  virtual Json::Value get_all_nodes() = 0;
  virtual Json::Value should_speak(const Json::Value &param1) = 0;
  virtual Json::Value place_vote(const Json::Value &param1) = 0;
  virtual Json::Value get_votes(const Json::Value &param1) = 0;
  virtual Json::Value draw_graph(const Json::Value &param1) = 0;
  virtual Json::Value get_transaction_count(const Json::Value &param1) = 0;
  virtual Json::Value get_executed_trx_count(const Json::Value &param1) = 0;
  virtual Json::Value get_executed_blk_count(const Json::Value &param1) = 0;

  virtual Json::Value get_dag_size(const Json::Value &param1) = 0;
  virtual Json::Value get_pbft_chain_size() = 0;
  virtual Json::Value get_pbft_chain_blocks(const Json::Value &param1) = 0;
};

}  // namespace taraxa::net

#endif  // TARAXA_NODE_NET_TEST_FACE_H_
