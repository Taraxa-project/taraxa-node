#include <boost/program_options.hpp>
#include <condition_variable>

#include "full_node.hpp"
#include "static_init.hpp"

using namespace taraxa;
using namespace std;

int main(int argc, const char* argv[]) {
  static_init();
  try {
    string conf_taraxa;
    bool destroy_db = 0;
    bool rebuild_network = 0;
    uint64_t revert_to_period = 0;
    boost::program_options::options_description main_options("GENERIC OPTIONS:");
    main_options.add_options()("help", "Print this help message and exit")("conf_taraxa", boost::program_options::value<string>(&conf_taraxa),
                                                                           "Config for taraxa node (either json file path or inline json) "
                                                                           "[required]")(
        "destroy_db", boost::program_options::bool_switch(&destroy_db), "Destroys all the existing data in the database")(
        "rebuild_network", boost::program_options::bool_switch(&rebuild_network),
        "Delete all saved network/nodes information and rebuild network "
        "from boot nodes")("revert_to_period", boost::program_options::value<uint64_t>(&revert_to_period),
                           "Revert db/state to specified period (specify period) ");
    boost::program_options::options_description allowed_options("Allowed options");
    allowed_options.add(main_options);
    boost::program_options::variables_map option_vars;
    auto parsed_line = boost::program_options::parse_command_line(argc, argv, allowed_options);
    boost::program_options::store(parsed_line, option_vars);
    boost::program_options::notify(option_vars);
    if (option_vars.count("help")) {
      cout << allowed_options << endl;
      return 1;
    }
    if (!option_vars.count("conf_taraxa")) {
      cerr << "Please specify full node configuration file "
              "[--conf_taraxa]..."
           << endl;
      return 1;
    }
    FullNodeConfig cfg(conf_taraxa);
    if (destroy_db) {
      fs::remove_all(cfg.db_path);
    }
    if (rebuild_network) {
      fs::remove_all(cfg.net_file_path());
    }
    cfg.test_params.db_revert_to_period = revert_to_period;
    FullNode::Handle node(cfg, true);
    cout << "Taraxa node started" << endl;
    // TODO graceful shutdown
    mutex mu;
    unique_lock l(mu);
    condition_variable().wait(l);
    cout << "Taraxa Node exited ..." << endl;
    return 0;
  } catch (taraxa::ConfigException const& e) {
    cerr << "Configuration exception: " << e.what() << endl;
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << endl;
  }
  return 1;
}
