#include <boost/program_options.hpp>

#include "app/app.hpp"
#include "cli/config.hpp"
#include "common/config_exception.hpp"
#include "common/static_init.hpp"
// #include "node/node.hpp"

using namespace taraxa;

int main(int argc, const char* argv[]) {
  static_init();
  try {
    {
      auto app = std::make_shared<App>(argc, argv);

      if (app->nodeConfigured()) {
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
