#include "top.hpp"

#include <libweb3jsonrpc/Eth.h>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>

#include "aleth/node_api.hpp"
#include "aleth/state_api.hpp"
#include "config.hpp"
#include "net/RpcServer.h"
#include "transaction_manager.hpp"

using namespace std;
using namespace taraxa;

Top::Top(int argc, const char* argv[]) {
  std::string conf_taraxa;
  bool boot_node = false;
  bool destroy_db = false;
  bool rebuild_network = false;
  float tests_speed = 1;
  boost::program_options::options_description main_options("GENERIC OPTIONS:");
  main_options.add_options()("help", "Print this help message and exit")(
      "conf_taraxa", boost::program_options::value<std::string>(&conf_taraxa),
      "Configure file for taraxa node [required]")(
      "boot_node", boost::program_options::bool_switch(&boot_node),
      "Flag to mark this node as boot node")(
      "destroy_db", boost::program_options::bool_switch(&destroy_db),
      "Destroys all the existing data in the database")(
      "rebuild_network", boost::program_options::bool_switch(&rebuild_network),
      "Delete all saved network/nodes information and rebuild network "
      "from boot nodes")("tests_speed",
                         boost::program_options::value<float>(&tests_speed),
                         "Ignore - used for running tests");
  boost::program_options::options_description allowed_options(
      "Allowed options");
  allowed_options.add(main_options);
  boost::program_options::variables_map option_vars;
  auto parsed_line =
      boost::program_options::parse_command_line(argc, argv, allowed_options);
  boost::program_options::store(parsed_line, option_vars);
  boost::program_options::notify(option_vars);
  if (option_vars.count("help")) {
    std::cout << allowed_options << std::endl;
    // TODO this should be handled by the caller
    return;
  }
  if (!option_vars.count("conf_taraxa")) {
    std::cout << "Please specify full node configuration file "
                 "[--conf_taraxa]..."
              << std::endl;
    // TODO this should be handled by the caller
    return;
  }
  taraxa::FullNodeConfig conf(conf_taraxa);
  if (tests_speed != 1) {
    conf.test_params.block_proposer.min_proposal_delay /= tests_speed;
    conf.test_params.block_proposer.difficulty_bound = 5;
    conf.test_params.block_proposer.lambda_bound = 100;
    conf.test_params.pbft.lambda_ms_min /= tests_speed;
  }
  node_ = taraxa::FullNode::make(conf,
                                 destroy_db,  //
                                 rebuild_network);
  node_->start(boot_node);
  auto rpc_init_done = make_shared<condition_variable>();
  rpc_thread_ = thread([this, rpc_init_done] {
    try {
      auto const& node_config = node_->getConfig();
      auto rpc_server =
          make_shared<net::RpcServer>(rpc_io_context_,  //
                                      node_config.rpc, node_->getAddress());
      auto final_chain = node_->getFinalChain();
      auto rpc = make_shared<ModularServer<net::TestFace,
                                           net::TaraxaFace,  //
                                           net::NetFace,     //
                                           dev::rpc::EthFace>>(
          new net::Test(node_),
          new net::Taraxa(node_),  //
          new net::Net(node_),
          new dev::rpc::Eth(
              aleth::NewNodeAPI(
                  node_config.chain.chain_id, node_->getSecretKey(),
                  [=](auto const& trx) {
                    auto result =
                        node_->getTransactionManager()->insertTransaction(trx,
                                                                          true);
                    if (!result.first) {
                      BOOST_THROW_EXCEPTION(runtime_error(
                          fmt("Transaction is rejected.\n"
                              "RLP: %s\n"
                              "Reason: %s",
                              dev::toJS(*trx.rlp()), result.second)));
                    }
                  }),
              node_->getTransactionManager()->getFilterAPI(),
              aleth::NewStateAPI(final_chain),  //
              node_->getTransactionManager()->getPendingBlock(),
              final_chain,  //
              [] { return 0; }));
      rpc->addConnector(rpc_server);
      rpc_server->StartListening();
      auto ws_listener = make_shared<net::WSServer>(
          rpc_io_context_,  //
          boost::asio::ip::tcp::endpoint{
              boost::asio::ip::make_address(
                  node_config.rpc.address.to_string()),
              node_config.rpc.ws_port,
          },
          node_->getAddress());
      node_->setWSServer(ws_listener);
      ws_listener->run();
      rpc_init_done->notify_one();
      rpc_io_context_.run();
    } catch (...) {
      rpc_init_done->notify_one();
      std::cerr << boost::diagnostic_information(boost::current_exception());
    }
  });
  {
    mutex m;
    unique_lock l(m);
    rpc_init_done->wait(l);
  }
}

void Top::join() {
  if (node_) {
    rpc_thread_.join();
  }
}

Top::~Top() {
  rpc_io_context_.stop();
  this->join();
}