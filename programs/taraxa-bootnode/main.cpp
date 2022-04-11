#include <libp2p/Common.h>
#include <libp2p/Host.h>

#include <boost/core/null_deleter.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "common/jsoncpp.hpp"
#include "common/thread_pool.hpp"
#include "config/version.hpp"

namespace po = boost::program_options;
namespace bi = boost::asio::ip;

namespace {
std::string const kProgramName = "taraxa-bootnode";
std::string const kNetworkConfigFileName = kProgramName + "-network.rlp";
static constexpr unsigned kLineWidth = 160;

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", int)

po::options_description createLoggingProgramOptions(dev::LoggingOptions& options) {
  po::options_description options_descr("LOGGING OPTIONS", kLineWidth);
  auto addLoggingOption = options_descr.add_options();
  addLoggingOption("log-verbosity,v", po::value<int>(&options.verbosity)->value_name("<0 - 4>"),
                   "Set the log verbosity from 0 to 4 (default: 2).");
  return options_descr;
}

void setupLogging(dev::LoggingOptions const& options) {
  auto sink = boost::make_shared<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>>();

  boost::shared_ptr<std::ostream> stream{&std::cout, boost::null_deleter{}};
  sink->locked_backend()->add_stream(stream);
  // Enable auto-flushing after each log record written
  sink->locked_backend()->auto_flush(true);

  sink->set_filter([options](boost::log::attribute_value_set const& set) {
    if (set[severity] > options.verbosity) return false;
    return true;
  });

  sink->set_formatter(boost::log::aux::acquire_formatter("%Channel% [%TimeStamp%] %SeverityStr%: %Message%"));

  boost::log::core::get()->add_sink(sink);
  boost::log::core::get()->add_global_attribute("TimeStamp", boost::log::attributes::local_clock());

  boost::log::core::get()->set_exception_handler(boost::log::make_exception_handler<std::exception>(
      [](std::exception const& _ex) { std::cerr << "Exception from the logging library: " << _ex.what() << '\n'; }));
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

  po::options_description general_options("GENERAL OPTIONS", kLineWidth);
  auto addGeneralOption = general_options.add_options();
  addGeneralOption("help,h", "Show this help message and exit\n");
  addGeneralOption("version", "Print version of taraxad");

  dev::LoggingOptions logging_options;
  po::options_description logging_program_options(createLoggingProgramOptions(logging_options));

  po::options_description client_networking("NETWORKING", kLineWidth);
  auto addNetworkingOption = client_networking.add_options();
  addNetworkingOption("public-ip", po::value<std::string>()->value_name("<ip>"),
                      "Force advertised public IP to the given IP (default: auto)");
  addNetworkingOption("listen-ip", po::value<std::string>()->value_name("<ip>(:<port>)"),
                      "Listen on the given IP for incoming connections (default: 0.0.0.0)");
  addNetworkingOption("listen", po::value<unsigned short>()->value_name("<port>"),
                      "Listen on the given port for incoming connections (default: 10002)");
  addNetworkingOption("deny-local-discovery", po::bool_switch(&denyLocalDiscovery),
                      "Reject local addresses in the discovery process. Used for testing purposes.");
  addNetworkingOption("network-id", po::value<unsigned short>()->value_name("<id>"),
                      "Connect to default mainet/testnet/devnet bootnodes");
  addNetworkingOption("number-of-threads", po::value<uint32_t>()->value_name("<#>"),
                      "Define number of threads for this bootnode (default: 1)");
  addNetworkingOption("wallet", po::value<std::string>(&wallet),
                      "JSON wallet file, if not specified key random generated");
  po::options_description allowedOptions("Allowed options");
  allowedOptions.add(general_options).add(logging_program_options).add(client_networking);

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
    std::cout << general_options << client_networking << logging_program_options;
    return 0;
  }

  if (vm.count("version")) {
    Json::Value version_json;
    version_json["version"] = TARAXA_VERSION;
    version_json["git_commit_hash"] = TARAXA_GIT_COMMIT_HASH;
    version_json["git_branch"] = TARAXA_GIT_BRANCH;
    version_json["git_description"] = TARAXA_GIT_DESCRIBE;
    std::cout << version_json << std::endl;
    return 0;
  }

  /// Networking params.
  unsigned short network_id = static_cast<unsigned short>(taraxa::cli::Config::DEFAULT_NETWORK_ID);
  if (vm.count("network-id")) network_id = vm["network-id"].as<unsigned short>();

  std::string listen_ip = "0.0.0.0";
  unsigned short listen_port = 10002;
  std::string public_ip;
  uint32_t num_of_threads = 1;

  if (vm.count("public-ip")) public_ip = vm["public-ip"].as<std::string>();
  if (vm.count("listen-ip")) listen_ip = vm["listen-ip"].as<std::string>();
  if (vm.count("listen")) listen_port = vm["listen"].as<unsigned short>();
  if (vm.count("number-of-threads")) num_of_threads = vm["number-of-threads"].as<uint32_t>();

  setupLogging(logging_options);
  if (logging_options.verbosity > 0)
    std::cout << EthGrayBold << kProgramName << ", a Taraxa bootnode implementation" EthReset << std::endl;

  auto key = dev::KeyPair(dev::Secret::random());
  if (!wallet.empty()) {
    try {
      key = getKey(wallet);
    } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      return 1;
    }
  }

  auto net_conf = public_ip.empty() ? dev::p2p::NetworkConfig(listen_ip, listen_port, false)
                                    : dev::p2p::NetworkConfig(public_ip, listen_ip, listen_port, false);
  net_conf.allowLocalDiscovery = !denyLocalDiscovery;

  dev::p2p::TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.is_boot_node = true;
  taraxa_net_conf.network_id = network_id;
  auto network_file_path = taraxa::cli::Tools::getTaraxaDefaultDir() / std::filesystem::path(kNetworkConfigFileName);

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
    std::cout << "Node ID: " << boot_host->enode() << std::endl;
    if (static_cast<taraxa::cli::Config::NetworkIdType>(network_id) <
        taraxa::cli::Config::NetworkIdType::LastNetworkId) {
      const auto conf = taraxa::cli::Tools::generateConfig(static_cast<taraxa::cli::Config::NetworkIdType>(network_id));
      for (auto const& bn : conf["network_boot_nodes"]) {
        bi::tcp::endpoint ep = dev::p2p::Network::resolveHost(bn["ip"].asString() + ":" + bn["udp_port"].asString());
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
  return 0;
}
