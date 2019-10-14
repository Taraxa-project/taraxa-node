#include "top.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include "config.hpp"
#include "libweb3jsonrpc/Net.h"
#include "libweb3jsonrpc/RpcServer.h"
#include "libweb3jsonrpc/Taraxa.h"

using namespace std;

Top::Top(int argc, const char* argv[]) {
  std::string conf_taraxa;
  bool boot_node = false;
  bool destroy_db = false;
  bool rebuild_network = false;
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
      "from boot nodes");
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
    // TODO
    assert(false);
  }
  if (!option_vars.count("conf_taraxa")) {
    std::cout << "Please specify full node configuration file "
                 "[--conf_taraxa]..."
              << std::endl;
    // TODO
    assert(false);
  }
  node_ = taraxa::FullNode::make(conf_taraxa,
                                 destroy_db,  //
                                 rebuild_network);
  node_->start(boot_node);
  auto rpc_init_done = make_shared<condition_variable>();
  rpc_thread_ = thread([this, rpc_init_done] {
    try {
      rpc_io_context_.post([&] { rpc_init_done->notify_one(); });
      auto const& node_config = node_->getConfig();
      auto rpc_server = make_shared<taraxa::RpcServer>(rpc_io_context_,  //
                                                       node_config.rpc);
      auto rpc = make_shared<Rpc>(new dev::rpc::Test(node_),
                                  new dev::rpc::Taraxa(node_),  //
                                  new dev::rpc::Net(node_));
      rpc->addConnector(rpc_server);
      rpc_server->StartListening();
      auto ws_listener = make_shared<taraxa::WSServer>(
          rpc_io_context_,  //
          tcp::endpoint{
              net::ip::make_address(node_config.rpc.address.to_string()),
              node_config.rpc.ws_port,
          });
      node_->setWSServer(ws_listener);
      ws_listener->run();
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
  // TODO remove after fixing the tests
  taraxa::thisThreadSleepForMilliSeconds(2000);
}

void Top::join() { rpc_thread_.join(); }

Top::~Top() {
  rpc_io_context_.stop();
  this->join();
}