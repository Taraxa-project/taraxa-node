#include <boost/program_options.hpp>
#include <condition_variable>

#include "cli/config.hpp"
#include "common/static_init.hpp"
#include "node/node.hpp"

using namespace taraxa;
using namespace std;

namespace bpo = boost::program_options;
std::shared_ptr<FullNode> node;

condition_variable stop_variable;

void SignalHandler(int signal) {
  std::cout << "Taraxa node interrupted by signal: " << std::signal << endl;
  node.reset();
  stop_variable.notify_all();
}

int main(int argc, const char* argv[]) {
  static_init();
  try {
    // Setup handler for SIGTERM
    struct sigaction SignalAction;
    memset(&SignalAction, 0, sizeof(SignalAction));
    SignalAction.sa_handler = SignalHandler;
    sigaction(SIGTERM, &SignalAction, NULL);
    sigaction(SIGINT, &SignalAction, NULL);

    cli::Config cli_conf(argc, argv);
    if (cli_conf.nodeConfigured()) {
      node = make_shared<FullNode>(cli_conf.getNodeConfiguration());
      node->start();

      if (node->isStarted()) {
        cout << "Taraxa node started" << endl;
        mutex mu;
        unique_lock l(mu);
        stop_variable.wait(l);
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
