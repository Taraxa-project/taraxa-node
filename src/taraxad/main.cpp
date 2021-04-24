#include <boost/program_options.hpp>
#include <condition_variable>

#include "common/static_init.hpp"
#include "node/full_node.hpp"
#include "taraxad_version.hpp"

using namespace taraxa;
using namespace std;

namespace bpo = boost::program_options;

int main(int argc, const char* argv[]) {
  static_init();

  try {
    string conf_taraxa;
    string conf_chain;
    bool destroy_db = 0;
    bool rebuild_network = 0;
    bool rebuild_db = 0;
    uint64_t rebuild_db_period = 0;
    uint64_t revert_to_period = 0;
    bpo::options_description main_options("GENERIC OPTIONS:");
    main_options.add_options()("help", "Print this help message and exit")("version", "Print version of taraxd")(
        "conf_taraxa", bpo::value<string>(&conf_taraxa),
        "Config for taraxa node (either json file path or inline json) [required]")(
        "destroy_db", bpo::bool_switch(&destroy_db), "Destroys all the existing data in the database")(
        "rebuild_db", bpo::bool_switch(&rebuild_db),
        "Reads the raw dag/pbft blocks from the db and executes all the blocks from scratch rebuilding all the other "
        "database tables - this could take a long time")("rebuild_db_period", bpo::value<uint64_t>(&rebuild_db_period),
                                                         "Use with rebuild_db - Rebuild db up to a specified period")(
        "rebuild_network", bpo::bool_switch(&rebuild_network),
        "Delete all saved network/nodes information and rebuild network "
        "from boot nodes")("revert_to_period", bpo::value<uint64_t>(&revert_to_period),
                           "Revert db/state to specified period (specify period) ");
    bpo::options_description allowed_options("Allowed options");
    allowed_options.add(main_options);
    bpo::variables_map option_vars;
    auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);
    bpo::store(parsed_line, option_vars);
    bpo::notify(option_vars);
    if (option_vars.count("help")) {
      cout << allowed_options << endl;
      return 1;
    }
    if (option_vars.count("version")) {
      cout << TARAXAD_VERSION << endl;
      return 1;
    }
    if (!option_vars.count("conf_taraxa")) {
      cerr << "Please specify full node configuration file "
              "[--conf_taraxa]..."
           << endl;
      return 1;
    }

    // Loads config
    FullNodeConfig cfg(conf_taraxa, conf_chain);

    // Validates config values
    if (!cfg.validate()) {
      cerr << "Invalid configration. Please make sure config values are valid";
      return 1;
    }

    if (destroy_db) {
      fs::remove_all(cfg.db_path);
    }
    if (rebuild_network) {
      fs::remove_all(cfg.net_file_path());
    }
    cfg.test_params.db_revert_to_period = revert_to_period;
    cfg.test_params.rebuild_db = rebuild_db;
    cfg.test_params.rebuild_db_period = rebuild_db_period;
    FullNode::Handle node(cfg, true);
    if (node->isStarted()) {
      cout << "Taraxa node started" << endl;
      // TODO graceful shutdown
      mutex mu;
      unique_lock l(mu);
      condition_variable().wait(l);
    }
    cout << "Taraxa Node exited ..." << endl;
    return 0;
  } catch (taraxa::ConfigException const& e) {
    cerr << "Configuration exception: " << e.what() << endl;
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << endl;
  }
  return 1;
}
