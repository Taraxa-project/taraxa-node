#include <boost/program_options.hpp>
#include <condition_variable>

#include "cli/config.hpp"
#include "common/static_init.hpp"
#include "node/node.hpp"

using namespace taraxa;
using namespace std;

namespace bpo = boost::program_options;

int main(int argc, const char* argv[]) {
  static_init();
  try {
    cli::Config cli_conf(argc, argv);

    if (cli_conf.nodeConfigured()) {
      auto node = make_shared<FullNode>(cli_conf.getNodeConfiguration());
      node->start();

      if (node->isStarted()) {
        cout << "Taraxa node started" << endl;
        // TODO graceful shutdown
        mutex mu;
        unique_lock l(mu);
        condition_variable().wait(l);
      }
      cout << "Taraxa Node exited ..." << endl;
    }
    return 0;
  } catch (taraxa::ConfigException const& e) {
    cerr << "Configuration exception: " << e.what() << endl;
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << endl;
  }
  return 1;
}
