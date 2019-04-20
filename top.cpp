/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-19 12:56:28
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-19 18:33:34
 */

#include "top.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>

Top::Top(int argc, const char* argv[]) { start(argc, argv); }

void Top::start(int argc, const char* argv[]) {
  if (!stopped_) return;
  stopped_ = false;
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << std::endl;
  }
  th_ = std::make_shared<std::thread>([this, argc, argv]() {
    bool verbose = false;
    std::string conf_full_node, conf_network, conf_rpc;
    // loggin options
    dev::LoggingOptions loggingOptions;
    boost::program_options::options_description loggingProgramOptions(
        dev::createLoggingProgramOptions(160, loggingOptions));

    boost::program_options::options_description main_options(
        "GENERIC OPTIONS:");
    main_options.add_options()("help", "Print this help message and exit")(
        "verbose", "Print more info")(
        "conf_full_node",
        boost::program_options::value<std::string>(&conf_full_node),
        "Configure file for full node [required]")(
        "conf_network",
        boost::program_options::value<std::string>(&conf_network),
        "Configure file for network [required]")(
        "conf_rpc", boost::program_options::value<std::string>(&conf_rpc),
        "Configure file for rpc [required]");

    boost::program_options::options_description allowed_options(
        "Allowed options");
    allowed_options.add(main_options).add(loggingProgramOptions);

    boost::program_options::variables_map option_vars;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, allowed_options),
        option_vars);
    boost::program_options::notify(option_vars);
    dev::setupLogging(loggingOptions);

    std::cout << "conf_full_node =" << conf_full_node << std::endl;
    if (option_vars.count("help")) {
      std::cout << allowed_options << std::endl;
      stopped_ = true;
    }
    if (option_vars.count("verbose") > 0) {
      stopped_ = true;
    }
    if (!option_vars.count("conf_network")) {
      std::cout << "Please specify full node configuration file "
                   "[--conf_full_node]..."
                << std::endl;
      stopped_ = true;
    }
    if (!option_vars.count("conf_network")) {
      std::cout
          << "Please specify network configuration file ... [--conf_network]"
          << std::endl;
      stopped_ = true;
    }
    if (!option_vars.count("conf_rpc")) {
      std::cout << "Please specify rpc configuration file ... [--conf_rpc]"
                << std::endl;
      stopped_ = true;
    }
    if (stopped_) return;
    stopped_ = false;
    try {
      node_ = std::make_shared<taraxa::FullNode>(context_, conf_full_node,
                                                 conf_network);
      node_->setVerbose(verbose);
      node_->start();
      rpc_ =
          std::make_shared<taraxa::Rpc>(context_, conf_rpc, node_->getShared());
      rpc_->start();
      context_.run();
    } catch (std::exception& e) {
      stopped_ = true;
      std::cerr << e.what();
    }
  });
  std::cout << "TOP CTOR done" << std::endl;
}
void Top::run() {
  std::unique_lock<std::mutex> lock(mu_);
  while (!stopped_) {
    cond_.wait(lock);
  }
}
void Top::stop() {
  if (stopped_) return;
  stopped_ = true;
  cond_.notify_all();
  node_->stop();
  rpc_->stop();
  std::cout << "Top::stop() called!" << std::endl;
}

Top::~Top() {
  // stop();
  th_->join();
  std::cout << "Top::~Top() called!" << std::endl;
}