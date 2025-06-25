#include <libp2p/Common.h>
#include <libp2p/Host.h>

#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <string>

#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "common/thread_pool.hpp"
#include "common/util.hpp"
#include "config/version.hpp"
#include "logger/logging.hpp"
#include "logger/logging_config.hpp"

namespace po = boost::program_options;
namespace bi = boost::asio::ip;

namespace {
std::string const kProgramName = "taraxa-bootnode";
std::string const kNetworkConfigFileName = kProgramName + "-network.rlp";
static constexpr unsigned kLineWidth = 160;

taraxa::LoggingConfig setupLogging(int logging_verbosity) {
  taraxa::LoggingConfig logging_config;

  taraxa::LoggingConfig::SinkConfig default_sink;
  default_sink.on = true;
  default_sink.verbosity = static_cast<spdlog::level::level_enum>(logging_verbosity);

  auto& console_sink = logging_config.outputs.emplace_back(default_sink);
  console_sink.type = taraxa::LoggingConfig::LoggingType::Console;
  console_sink.name = "console";

  auto& file_sink = logging_config.outputs.emplace_back(default_sink);
  console_sink.type = taraxa::LoggingConfig::LoggingType::File;
  console_sink.name = "file";
  console_sink.file_name = "boot-node.log";
  console_sink.file_full_path = console_sink.file_name;
  file_sink.rotation_size = 10000000;
  file_sink.max_files_num = 10;

  return logging_config;
}

dev::KeyPair getKey(std::string const& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Wallet file does not exist at: " + path);
  }

  auto wallet_json = taraxa::util::readJsonFromFile(path);
  if (wallet_json["node_secret"].isNull()) {
    throw std::runtime_error("Wallet file does not contain node_secret field");
  }

  auto sk = dev::Secret(wallet_json["node_secret"].asString(), dev::Secret::ConstructFromStringType::FromHex);
  return dev::KeyPair(sk);
}
}  // namespace

int main(int argc, char** argv) {
  bool denyLocalDiscovery;
  std::string wallet;
  int logging_verbosity;

  po::options_description general_options("GENERAL OPTIONS", kLineWidth);
  auto addGeneralOption = general_options.add_options();
  addGeneralOption("help,h", "Show this help message and exit\n");
  addGeneralOption("version", "Print version of taraxad");

  po::options_description logging_options("LOGGING OPTIONS", kLineWidth);
  auto addLoggingOption = logging_options.add_options();
  addLoggingOption("log-verbosity,v", po::value<int>(&logging_verbosity)->value_name("<0 - 4>"),
                   "Set the log verbosity from 0 to 4 (default: 2).");

  po::options_description client_networking("NETWORKING", kLineWidth);
  auto addNetworkingOption = client_networking.add_options();
  addNetworkingOption("public-ip", po::value<std::string>()->value_name("<ip>"),
                      "Force advertised public IP to the given IP (default: auto)");
  addNetworkingOption("listen-ip", po::value<std::string>()->value_name("<ip>"),
                      "Listen on the given IP for incoming connections (default: 0.0.0.0)");
  addNetworkingOption("listen", po::value<uint16_t>()->value_name("<port>"),
                      "Listen on the given port for incoming connections (default: 10002)");
  addNetworkingOption("deny-local-discovery", po::bool_switch(&denyLocalDiscovery),
                      "Reject local addresses in the discovery process. Used for testing purposes.");
  addNetworkingOption("chain-id", po::value<uint32_t>()->value_name("<id>"),
                      "Connect to default mainet/testnet/devnet bootnodes");
  addNetworkingOption("number-of-threads", po::value<uint32_t>()->value_name("<#>"),
                      "Define number of threads for this bootnode (default: 1)");
  addNetworkingOption("wallet", po::value<std::string>(&wallet),
                      "JSON wallet file, if not specified key random generated");
  po::options_description allowedOptions("Allowed options");
  allowedOptions.add(general_options).add(logging_options).add(client_networking);

  po::variables_map vm;
  try {
    po::parsed_options parsed = po::parse_command_line(argc, argv, allowedOptions);
    po::store(parsed, vm);
    po::notify(vm);
  } catch (po::error const& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  if (vm.count("help")) {
    std::cout << "NAME:\n"
              << "   " << kProgramName << std::endl
              << "USAGE:\n"
              << "   " << kProgramName << " [options]\n\n";
    std::cout << general_options << client_networking << logging_options;
    return 0;
  }

  if (vm.count("version")) {
    std::cout << kVersionJson << std::endl;
    return 0;
  }

  /// Networking params.
  uint32_t chain_id = static_cast<uint32_t>(taraxa::cli::Config::DEFAULT_CHAIN_ID);
  if (vm.count("chain-id")) chain_id = vm["chain-id"].as<uint32_t>();

  std::string listen_ip = "0.0.0.0";
  uint16_t listen_port = 10002;
  std::string public_ip;
  uint32_t num_of_threads = 1;

  if (vm.count("public-ip")) public_ip = vm["public-ip"].as<std::string>();
  if (vm.count("listen-ip")) listen_ip = vm["listen-ip"].as<std::string>();
  if (vm.count("listen")) listen_port = vm["listen"].as<uint16_t>();
  if (vm.count("number-of-threads")) num_of_threads = vm["number-of-threads"].as<uint32_t>();

  const auto logging_config = setupLogging(logging_verbosity);
  taraxa::logger::Logging::get().Init(logging_config);
  auto logger = taraxa::logger::Logging::get().CreateChannelLogger("BOOTNODE");
  logger->info("{}, a Taraxa bootnode implementation", kProgramName);

  auto key = dev::KeyPair(dev::Secret::random());
  if (!wallet.empty()) {
    try {
      key = getKey(wallet);
    } catch (std::exception const& e) {
      logger->error("Wallet key err: ", e.what());
      return 1;
    }
  }

  auto net_conf = public_ip.empty() ? dev::p2p::NetworkConfig(listen_ip, listen_port, false)
                                    : dev::p2p::NetworkConfig(public_ip, listen_ip, listen_port, false, false);
  net_conf.allowLocalDiscovery = !denyLocalDiscovery;

  dev::p2p::TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.is_boot_node = true;
  taraxa_net_conf.chain_id = chain_id;
  auto network_file_path = taraxa::cli::tools::getTaraxaDefaultDir() / std::filesystem::path(kNetworkConfigFileName);

  auto boot_host = dev::p2p::Host::make(
      kProgramName, [](auto /*host*/) { return dev::p2p::Host::CapabilityList{}; }, key, net_conf, taraxa_net_conf,
      network_file_path);

  taraxa::util::ThreadPool tp{num_of_threads};
  for (uint i = 0; i < tp.capacity(); ++i) {
    tp.post_loop({i * 20}, [boot_host] {
      if (!boot_host->do_discov()) taraxa::thisThreadSleepForMilliSeconds(500);
    });
  }

  if (boot_host->isRunning()) {
    logger->info("Node ID: {}", boot_host->enode());
    if (static_cast<taraxa::cli::Config::ChainIdType>(chain_id) < taraxa::cli::Config::ChainIdType::LastNetworkId) {
      const auto conf = taraxa::cli::tools::getConfig(static_cast<taraxa::cli::Config::ChainIdType>(chain_id));
      for (auto const& bn : conf["network"]["boot_nodes"]) {
        bi::tcp::endpoint ep = dev::p2p::Network::resolveHost(bn["ip"].asString() + ":" + bn["port"].asString());
        boot_host->addNode(
            dev::p2p::Node(dev::Public(bn["id"].asString()),
                           dev::p2p::NodeIPEndpoint{ep.address(), ep.port() /* udp */, ep.port() /* tcp */},
                           dev::p2p::PeerType::Required));
      }
    }

    // TODO graceful shutdown
    std::mutex mu;
    std::unique_lock l(mu);
    std::condition_variable().wait(l);
  }

  taraxa::logger::Logging::get().Deinit();
  return 0;
}
