#include "top.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include "config.hpp"
#include "libweb3jsonrpc/Net.h"
#include "libweb3jsonrpc/RpcServer.h"
#include "libweb3jsonrpc/Taraxa.h"

Top::Top(int argc, const char* argv[]) { start(argc, argv); }

void Top::start(int argc, const char* argv[]) {
  if (!stopped_) return;
  if (!th_) {
    stopped_ = false;
    th_ = std::make_shared<std::thread>([this, argc, argv]() {
      try {
        bool verbose = false;
        std::string conf_taraxa;
        bool boot_node = false;
        bool destroy_db = false;
        bool rebuild_network = false;
        // loggin options
        dev::LoggingOptions loggingOptions;
        boost::program_options::options_description loggingProgramOptions(
            dev::createLoggingProgramOptions(160, loggingOptions));
        boost::program_options::options_description main_options(
            "GENERIC OPTIONS:");
        main_options.add_options()("help", "Print this help message and exit")(
            "conf_taraxa",
            boost::program_options::value<std::string>(&conf_taraxa),
            "Configure file for taraxa node [required]")(
            "boot_node", boost::program_options::bool_switch(&boot_node),
            "Flag to mark this node as boot node")(
            "destroy_db", boost::program_options::bool_switch(&destroy_db),
            "Destroys all the existing data in the database")(
            "rebuild_network",
            boost::program_options::bool_switch(&rebuild_network),
            "Delete all saved network/nodes information and rebuild network "
            "from boot nodes");

        boost::program_options::options_description allowed_options(
            "Allowed options");
        allowed_options.add(main_options).add(loggingProgramOptions);

        boost::program_options::variables_map option_vars;
        auto parsed_line = boost::program_options::parse_command_line(
            argc, argv, allowed_options);
        boost::program_options::store(parsed_line, option_vars);
        boost::program_options::notify(option_vars);
        dev::setupLogging(loggingOptions);

        if (option_vars.count("help")) {
          std::cout << allowed_options << std::endl;
          stopped_ = true;
        }
        if (!option_vars.count("conf_taraxa")) {
          std::cout << "Please specify full node configuration file "
                       "[--conf_taraxa]..."
                    << std::endl;
          stopped_ = true;
        }
        if (stopped_) {
          return;
        }
        stopped_ = false;
        boot_node_ = boot_node;

        conf_ = std::make_shared<taraxa::FullNodeConfig>(conf_taraxa);
        node_ = std::make_shared<taraxa::FullNode>(context_, *conf_, destroy_db,
                                                   rebuild_network);
        node_->start(boot_node_);
        startRpc();
        context_.run();
      } catch (...) {
        stopped_ = true;
        exceptions_.push(boost::current_exception());
      }
    });
  } else {
    start();
  }

  // Important!!! Wait for a while and have the child thread initialize
  // everything ... Otherwise node_ will get nullptr
  if (!stopped_) {
    taraxa::thisThreadSleepForSeconds(5);
  }
}

void Top::startRpc() {
  rpc_ =
      std::make_shared<ModularServer<dev::rpc::TestFace, dev::rpc::TaraxaFace,
                                     dev::rpc::NetFace> >(
          new dev::rpc::Test(node_), new dev::rpc::Taraxa(node_),
          new dev::rpc::Net(node_));
  auto rpc_server(
      std::make_shared<taraxa::RpcServer>(context_, conf_->rpc, node_));
  rpc_->addConnector(rpc_server);
  rpc_server->StartListening();
  ws_listener_ = std::make_shared<taraxa::WSServer>(
      context_,
      tcp::endpoint{net::ip::make_address(conf_->rpc.address.to_string()),
                    conf_->rpc.ws_port});
  node_->setWSServer(ws_listener_);
  ws_listener_->run();
}

void Top::start() {
  if (!stopped_) return;
  stopped_ = false;
  assert(node_);
  node_->start(boot_node_);
}
void Top::run() {
  std::unique_lock<std::mutex> lock(mu_);
  while (!stopped_) {
    cond_.wait(lock);
  }
}
void Top::kill() {
  if (stopped_) return;
  stop();
  rpc_->StopListening();
  ws_listener_->stop();
  cond_.notify_all();
}
void Top::stop() {
  if (stopped_) return;
  stopped_ = true;
  node_->stop();
}
void Top::reset() {
  if (!stopped_) return;
  node_->reset();
}
Top::~Top() {
  kill();
  th_->join();
  boost::exception_ptr e;
  while (!exceptions_.empty()) {
    e = exceptions_.front();
    exceptions_.pop();
    std::cerr << boost::diagnostic_information(e);
  }
}