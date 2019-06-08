/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-19 12:56:28
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 17:05:26
 */

#include "top.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include "config.hpp"

Top::Top(int argc, const char* argv[]) { start(argc, argv); }

void Top::start(int argc, const char* argv[]) {
  if (!stopped_) return;
  if (!th_) {
    stopped_ = false;
    th_ = std::make_shared<std::thread>([this, argc, argv]() {
      bool verbose = false;
      std::string conf_taraxa;
      bool boot_node = false;
      bool destroy_db = false;
      // loggin options
      dev::LoggingOptions loggingOptions;
      boost::program_options::options_description loggingProgramOptions(
          dev::createLoggingProgramOptions(160, loggingOptions));

      boost::program_options::options_description main_options(
          "GENERIC OPTIONS:");
      main_options.add_options()("help", "Print this help message and exit")(
          "verbose", "Print more info")(
          "conf_taraxa",
          boost::program_options::value<std::string>(&conf_taraxa),
          "Configure file for taraxa node [required]")(
          "boot_node", boost::program_options::bool_switch(&boot_node),
          "Flag to mark this node as boot node")(
          "destroy_db", boost::program_options::bool_switch(&destroy_db),
          "Destroys all the existing data in the database");

      boost::program_options::options_description allowed_options(
          "Allowed options");
      allowed_options.add(main_options).add(loggingProgramOptions);

      boost::program_options::variables_map option_vars;
      boost::program_options::store(boost::program_options::parse_command_line(
                                        argc, argv, allowed_options),
                                    option_vars);
      boost::program_options::notify(option_vars);
      dev::setupLogging(loggingOptions);

      if (option_vars.count("help")) {
        std::cout << allowed_options << std::endl;
        stopped_ = true;
      }
      if (option_vars.count("verbose") > 0) {
        stopped_ = true;
      }
      if (!option_vars.count("conf_taraxa")) {
        std::cout << "Please specify full node configuration file "
                     "[--conf_taraxa]..."
                  << std::endl;
        stopped_ = true;
      }
      if (stopped_) return;
      stopped_ = false;
      boot_node_ = boot_node;
      try {
        taraxa::FullNodeConfig conf(conf_taraxa);
        node_ = std::make_shared<taraxa::FullNode>(context_, conf, destroy_db);
        node_->setVerbose(verbose);
        node_->start(boot_node_);
        rpc_ = std::make_shared<taraxa::Rpc>(context_, conf.rpc,
                                             node_->getShared());
        rpc_->start();
        context_.run();
      } catch (std::exception& e) {
        stopped_ = true;
        std::cerr << e.what();
      }
    });
  } else {
    start();
  }
  // Important!!! Wait for a while and have the child thread initialize
  // everything ... Otherwise node_ will get nullptr
  taraxa::thisThreadSleepForSeconds(2);
}
void Top::start() {
  if (!stopped_) return;
  stopped_ = false;
  assert(node_);
  node_->start(boot_node_);
  rpc_->start();
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
  rpc_->stop();
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
}