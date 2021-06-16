#include <boost/program_options.hpp>
#include <condition_variable>

#include "common/static_init.hpp"
#include "node/full_node.hpp"
#include "taraxad_version.hpp"
#include "cli/config.hpp"

using namespace taraxa;
using namespace std;

namespace bpo = boost::program_options;

int main(int argc, const char* argv[]) {
  static_init();
  try {
    cli::Config cli_conf(argc, argv, TARAXAD_VERSION);

    if(cli_conf.nodeConfigured()) {
      FullNode::Handle node(cli_conf.getNodeConfiguration(), true);
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
