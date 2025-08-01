#include <boost/program_options.hpp>

#include "app/app.hpp"
#include "cli/config.hpp"
#include "cli/tools.hpp"
#include "common/config_exception.hpp"
#include "common/init.hpp"
#include "plugin/rpc.hpp"

using namespace taraxa;

int main(int argc, const char* argv[]) {
  static_init();

  if (!checkDiskSpace(cli::tools::getTaraxaDataDefaultDir(), 512)) {
    std::cerr << "Insufficient disk space" << std::endl;
    return 1;
  }
  try {
    {
      auto app = std::make_shared<App>();

      cli::Config cli_conf;
      app->registerPlugin<plugin::Rpc>(cli_conf);

      cli_conf.parseCommandLine(argc, argv, app->registeredPlugins());

      if (cli_conf.nodeConfigured()) {
        for (const auto& plugin : cli_conf.getEnabledPlugins()) {
          app->enablePlugin(plugin);
        }
        app->init(cli_conf);
        app->start();
      }

      if (app->isStarted()) {
        std::cout << "Taraxa node started" << std::endl;
        // TODO graceful shutdown
        std::mutex mu;
        std::unique_lock l(mu);
        std::condition_variable().wait(l);
      }
    }
    std::cout << "Taraxa Node exited ..." << std::endl;
    return 0;
  } catch (taraxa::ConfigException const& e) {
    std::cerr << "Configuration exception: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << boost::current_exception_diagnostic_information() << std::endl;
  }
  return 1;
}
