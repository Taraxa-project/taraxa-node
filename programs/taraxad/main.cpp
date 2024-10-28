#include <boost/program_options.hpp>
#include <condition_variable>

#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "common/config_exception.hpp"
#include "common/init.hpp"
#include "node/node.hpp"

using namespace taraxa;

int main(int argc, const char* argv[]) {
  static_init();

  if (!checkDiskSpace(cli::tools::getTaraxaDefaultConfigFile(), 10)) {
    std::cerr << "Insufficient disk space" << std::endl;
    return 1;
  }

  try {
    cli::Config cli_conf;
    cli_conf.parseCommandLine(argc, argv);

    if (cli_conf.nodeConfigured()) {
      auto node = std::make_shared<FullNode>(cli_conf.getNodeConfiguration());
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
