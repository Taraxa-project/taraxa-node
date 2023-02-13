#include <boost/program_options.hpp>
#include <condition_variable>

#include "cli/config_parser.hpp"
#include "common/static_init.hpp"
#include "node/node.hpp"

using namespace taraxa;

namespace bpo = boost::program_options;

int main(int argc, const char* argv[]) {
  static_init();
  try {
    const auto config = cli::ConfigParser().createConfig(argc, argv);

    if (config.has_value()) {
      auto node = std::make_shared<FullNode>(*config);
      node->start();

      if (node->isStarted()) {
        std::cout << "Taraxa node started" << std::endl;
        // TODO graceful shutdown
        std::mutex mu;
        std::unique_lock l(mu);
        std::condition_variable().wait(l);
      }
      std::cout << "Taraxa Node exited ..." << std::endl;
    }
    return 0;
  } catch (taraxa::ConfigException const& e) {
    std::cerr << "Configuration exception: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
  }
  return 1;
}
