#include "top.hpp"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include "config.hpp"
#include "eth/eth.hpp"
#include "net/RpcServer.h"

using namespace std;

Top::Top(int argc, const char* argv[]) {
  std::string conf_taraxa;
  bool boot_node = false;
  bool destroy_db = false;
  bool rebuild_network = false;
  float tests_speed = 1;
  // loggin options
  dev::LoggingOptions loggingOptions;
  boost::program_options::options_description loggingProgramOptions(
      dev::createLoggingProgramOptions(160, loggingOptions));
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
  allowed_options.add(main_options).add(loggingProgramOptions);
  boost::program_options::variables_map option_vars;
  auto parsed_line =
      boost::program_options::parse_command_line(argc, argv, allowed_options);
  boost::program_options::store(parsed_line, option_vars);
  boost::program_options::notify(option_vars);
  dev::setupLogging(loggingOptions);
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
    conf.test_params.block_proposer.min_freq /= tests_speed;
    conf.test_params.block_proposer.max_freq /= tests_speed;
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
      auto rpc_server = make_shared<taraxa::net::RpcServer>(rpc_io_context_,  //
                                                            node_config.rpc);
      auto eth_service = node_->getEthService();
      auto rpc = make_shared<ModularServer<taraxa::net::TestFace,
                                           taraxa::net::TaraxaFace,  //
                                           taraxa::net::NetFace,     //
                                           dev::rpc::EthFace>>(
          new taraxa::net::Test(node_),
          new taraxa::net::Taraxa(node_),  //
          new taraxa::net::Net(node_),
          new taraxa::eth::eth::Eth(eth_service,
                                    eth_service->current_node_account_holder));
      rpc->addConnector(rpc_server);
      rpc_server->StartListening();
      auto ws_listener = make_shared<taraxa::net::WSServer>(
          rpc_io_context_,  //
          boost::asio::ip::tcp::endpoint{
              boost::asio::ip::make_address(
                  node_config.rpc.address.to_string()),
              node_config.rpc.ws_port,
          });
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