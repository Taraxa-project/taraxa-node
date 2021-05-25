#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <condition_variable>

#include "common/static_init.hpp"
#include "node/full_node.hpp"
#include "taraxad_version.hpp"

using namespace taraxa;
using namespace std;

namespace bpo = boost::program_options;

enum networks { Mainnet = 1, Testnet, Devnet };

string getHomeDir() {
  struct passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

  return string(homedir);
}

string getTaraxaDefaultDir() {
  string home_dir = getHomeDir();
  auto taraxa_dir = home_dir + "/.taraxa";
  fs::create_directory(taraxa_dir);

  return taraxa_dir;
}

string getAccountFile() { return getTaraxaDefaultDir() + "/account.json"; }

string getVrfFile() { return getTaraxaDefaultDir() + "/vrf.json"; }

bool fileExist(string file) {
  ifstream ifile;
  ifile.open(file);
  if (ifile) {
    ifile.close();
    return true;
  } else {
    return false;
  }
}

void writeFile(string file_name, Json::Value json) {
  ofstream ofile(file_name);

  if (ofile.is_open()) {
    ofile << json;
  } else {
    stringstream err;
    err << "Cannot open file " << file_name << endl;
    throw invalid_argument(err.str());
  }
}

Json::Value getAccountJson(KeyPair account) {
  Json::Value json(Json::objectValue);
  json["node_secret"] = toHex(account.secret().ref());
  json["node_public"] = account.pub().toString();
  json["node_address"] = account.address().toString();

  return json;
}

Json::Value getVrfJson(taraxa::vrf_wrapper::vrf_sk_t sk, taraxa::vrf_wrapper::vrf_pk_t pk) {
  Json::Value json(Json::objectValue);
  json["vrf_secret"] = sk.toString();
  json["vrf_public"] = pk.toString();

  return json;
}

Json::Value getAccountSecretKey() {
  auto account_file = getAccountFile();
  ifstream ifile(account_file);
  if (ifile.is_open()) {
    Json::Value account_json;
    ifile >> account_json;
    ifile.close();
    return account_json["node_secret"];
  } else {
    stringstream err;
    err << "Cannot open file " << account_file << endl;
    throw invalid_argument(err.str());
  }
}

Json::Value getVrfSecretKey() {
  auto vrf_file = getVrfFile();
  ifstream ifile(vrf_file);
  if (ifile.is_open()) {
    Json::Value vrf_json;
    ifile >> vrf_json;
    ifile.close();
    return vrf_json["vrf_secret"];
  } else {
    stringstream err;
    err << "Cannot open file " << vrf_file << endl;
    throw invalid_argument(err.str());
  }
}

Json::Value mainnetBootNodes() { throw invalid_argument("Mainnet has not supported yet"); }

Json::Value testnetBootNodes() {
  Json::Value network_boot_nodes(Json::arrayValue);

  Json::Value boot_node1(Json::objectValue);
  boot_node1["id"] =
      "1105725ff38294f42197a3851c87631610956404678a25f90da5f989bb590413a62665cadde1b820de3e9098367909110bba6d62d248"
      "444ce45547a703bd8d10";
  boot_node1["ip"] = "boot-node-0.testnet.taraxa.io";
  boot_node1["tcp_port"] = 10002;
  boot_node1["udp_port"] = 10002;
  network_boot_nodes.append(boot_node1);

  Json::Value boot_node2(Json::objectValue);
  boot_node2["id"] =
      "9869d8c263ce97d2e32c7ce5e746a8bab53227ccf279f850bbf9def1ddb7fbc08dfb29e8f1ceb93c5b83d08e06618e0993c6db7e55ca"
      "a05a44605bdb74fbb0da";
  boot_node2["ip"] = "boot-node-1.testnet.taraxa.io";
  boot_node2["tcp_port"] = 10002;
  boot_node2["udp_port"] = 10002;
  network_boot_nodes.append(boot_node2);

  Json::Value boot_node3(Json::objectValue);
  boot_node3["id"] =
      "d7c8b4a8cc43549cdbb0f40c090559ae5d2584ba25aed95913f10fae9bceb17421f3250ee95126f33a77f679f4c7e806a0a9e643aa5a"
      "aec723f9e3155e85a820";
  boot_node3["ip"] = "boot-node-2.testnet.taraxa.io";
  boot_node3["tcp_port"] = 10002;
  boot_node3["udp_port"] = 10002;
  network_boot_nodes.append(boot_node3);

  return network_boot_nodes;
}

Json::Value devnetBootNodes() {
  Json::Value network_boot_nodes(Json::arrayValue);

  Json::Value boot_node1(Json::objectValue);
  boot_node1["id"] =
      "1105725ff38294f42197a3851c87631610956404678a25f90da5f989bb590413a62665cadde1b820de3e9098367909110bba6d62d248444c"
      "e45547a703bd8d10";
  boot_node1["ip"] = "boot-node-1.devnet.taraxa.io";
  boot_node1["tcp_port"] = 10002;
  boot_node1["udp_port"] = 10002;
  network_boot_nodes.append(boot_node1);

  Json::Value boot_node2(Json::objectValue);
  boot_node2["id"] =
      "9869d8c263ce97d2e32c7ce5e746a8bab53227ccf279f850bbf9def1ddb7fbc08dfb29e8f1ceb93c5b83d08e06618e0993c6db7e55caa05a"
      "44605bdb74fbb0da";
  boot_node2["ip"] = "boot-node-2.devnet.taraxa.io";
  boot_node2["tcp_port"] = 10002;
  boot_node2["udp_port"] = 10002;
  network_boot_nodes.append(boot_node2);

  Json::Value boot_node3(Json::objectValue);
  boot_node3["id"] =
      "d7c8b4a8cc43549cdbb0f40c090559ae5d2584ba25aed95913f10fae9bceb17421f3250ee95126f33a77f679f4c7e806a0a9e643aa5aaec7"
      "23f9e3155e85a820";
  boot_node3["ip"] = "boot-node-3.devnet.taraxa.io";
  boot_node3["tcp_port"] = 10002;
  boot_node3["udp_port"] = 10002;
  network_boot_nodes.append(boot_node3);

  return network_boot_nodes;
}

Json::Value getNetworkBootNodes(int network_identifier) {
  switch (network_identifier) {
    case Mainnet: {
      return mainnetBootNodes();
    }
    case Testnet: {
      return testnetBootNodes();
    }
    case Devnet: {
      return devnetBootNodes();
    }
    default: {
      stringstream err;
      err << "Wrong network identifier " << network_identifier << " (1=Mainnet, 2=Testnet, 3=Devnet)" << endl;
      throw invalid_argument(err.str());
    }
  }
}

Json::Value generateLogging() {
  Json::Value logging(Json::objectValue);
  auto &configurations = logging["configurations"] = Json::Value(Json::arrayValue);

  Json::Value standard(Json::objectValue);
  standard["name"] = "standard";
  standard["on"] = true;
  standard["verbosity"] = "INFO";

  auto &channels = standard["channels"] = Json::Value(Json::arrayValue);
  Json::Value summary(Json::objectValue);
  summary["name"] = "SUMMARY";
  summary["verbosity"] = "INFO";
  channels.append(summary);
  Json::Value full_node(Json::objectValue);
  full_node["name"] = "FULLND";
  full_node["verbosity"] = "INFO";
  channels.append(full_node);
  Json::Value pbft_mgr(Json::objectValue);
  pbft_mgr["name"] = "PBFT_MGR";
  pbft_mgr["verbosity"] = "INFO";
  channels.append(pbft_mgr);

  auto &outputs = standard["outputs"] = Json::Value(Json::arrayValue);
  Json::Value console(Json::objectValue);
  console["type"] = "console";
  console["format"] = "%NodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%";
  outputs.append(console);
  Json::Value file(Json::objectValue);
  file["type"] = "file";
  file["file_name"] = "Taraxa_N1_%m%d%Y_%H%M%S_%5N.log";
  file["rotation_size"] = 10000000;
  file["time_based_rotation"] = "0,0,0";
  file["format"] = "%NodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%";
  file["max_size"] = 1000000000;
  outputs.append(file);

  configurations.append(standard);

  return logging;
}

Json::Value mainnetChainConfig() { throw invalid_argument("Mainnet has not supported yet"); }

Json::Value testnetChainConfig() {
  Json::Value chain_config(Json::objectValue);
  chain_config["chain_id"] = "0x2";

  auto &dag_genesis_block = chain_config["dag_genesis_block"] = Json::Value(Json::objectValue);
  dag_genesis_block["level"] = "0x0";
  dag_genesis_block["pivot"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
  dag_genesis_block["sig"] =
      "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b26530"
      "5d046a52236c0a363701";
  dag_genesis_block["timestamp"] = "0x608c9a00";
  dag_genesis_block["tips"] = Json::Value(Json::arrayValue);
  dag_genesis_block["transactions"] = Json::Value(Json::arrayValue);

  auto &final_chain = chain_config["final_chain"] = Json::Value(Json::objectValue);

  auto &genesis_block_fields = final_chain["genesis_block_fields"] = Json::Value(Json::objectValue);
  genesis_block_fields["author"] = "0x0000000000000000000000000000000000000000";
  genesis_block_fields["timestamp"] = "0x608c9a00";

  auto &state = final_chain["state"] = Json::Value(Json::objectValue);
  state["disable_block_rewards"] = true;

  auto &dpos = state["dpos"] = Json::Value(Json::objectValue);
  dpos["deposit_delay"] = "0x5";
  dpos["withdrawal_delay"] = "0x5";
  dpos["eligibility_balance_threshold"] = "0xf4240";

  auto &genesis_state = dpos["genesis_state"] = Json::Value(Json::objectValue);
  auto delegate_from = "0x6c05d6e367a8c798308efbf4cefc1a18921a6f89";
  auto &delegate_to = genesis_state[delegate_from] = Json::Value(Json::objectValue);
  delegate_to["0x18551e353aa65bc0ffbdf9d93b7ad4a8fe29cf95"] = "0xf4240";
  delegate_to["0xc578bb5fc3dac3e96a8c4cb126c71d2dc9082817"] = "0xf4240";
  delegate_to["0x5c9afb23fba3967ca6102fb60c9949f6a38cd9e8"] = "0xf4240";

  auto &eth_chain_config = state["eth_chain_config"] = Json::Value(Json::objectValue);
  eth_chain_config["byzantium_block"] = "0x0";
  eth_chain_config["constantinople_block"] = "0x0";
  eth_chain_config["dao_fork_block"] = "0xffffffffffffffff";
  eth_chain_config["eip_150_block"] = "0x0";
  eth_chain_config["eip_158_block"] = "0x0";
  eth_chain_config["homestead_block"] = "0x0";
  eth_chain_config["petersburg_block"] = "0x0";

  auto &execution_options = state["execution_options"] = Json::Value(Json::objectValue);
  execution_options["disable_gas_fee"] = false;
  execution_options["disable_nonce_check"] = true;

  auto &genesis_balances = state["genesis_balances"] = Json::Value(Json::objectValue);
  auto addr1 = "6c05d6e367a8c798308efbf4cefc1a18921a6f89";
  auto addr2 = "f4a52b8f6dc8ab046fec6ad02e77023c044342e4";
  genesis_balances[addr1] = "0x1027e72f1f12813088000000";
  genesis_balances[addr2] = "0x1027e72f1f12813088000000";

  auto &pbft = chain_config["pbft"] = Json::Value(Json::objectValue);
  pbft["committee_size"] = "0x3e8";
  pbft["dag_blocks_size"] = "0xa";
  pbft["ghost_path_move_back"] = "0x0";
  pbft["lambda_ms_min"] = "0x29a";
  pbft["run_count_votes"] = false;

  auto &vdf = chain_config["vdf"] = Json::Value(Json::objectValue);
  vdf["difficulty_max"] = "0x12";
  vdf["difficulty_min"] = "0x10";
  vdf["threshold_selection"] = "0xbffd";
  vdf["threshold_vdf_omit"] = "0x6bf7";
  vdf["difficulty_stale"] = "0x13";
  vdf["lambda_bound"] = "0x64";

  return chain_config;
}

Json::Value devnetChainConfig() {
  Json::Value chain_config(Json::objectValue);
  chain_config["chain_id"] = "0x3";

  auto &dag_genesis_block = chain_config["dag_genesis_block"] = Json::Value(Json::objectValue);
  dag_genesis_block["level"] = "0x0";
  dag_genesis_block["pivot"] = "0x0000000000000000000000000000000000000000000000000000000000000000";
  dag_genesis_block["sig"] =
      "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b26530"
      "5d046a52236c0a363701";
  dag_genesis_block["timestamp"] = "0x5d422b84";
  dag_genesis_block["tips"] = Json::Value(Json::arrayValue);
  dag_genesis_block["transactions"] = Json::Value(Json::arrayValue);

  auto &final_chain = chain_config["final_chain"] = Json::Value(Json::objectValue);

  auto &genesis_block_fields = final_chain["genesis_block_fields"] = Json::Value(Json::objectValue);
  genesis_block_fields["author"] = "0x0000000000000000000000000000000000000000";
  genesis_block_fields["timestamp"] = "0x5d422b84";

  auto &state = final_chain["state"] = Json::Value(Json::objectValue);
  state["disable_block_rewards"] = true;

  auto &dpos = state["dpos"] = Json::Value(Json::objectValue);
  dpos["deposit_delay"] = "0x5";
  dpos["withdrawal_delay"] = "0x5";
  dpos["eligibility_balance_threshold"] = "0xf4240";

  auto &genesis_state = dpos["genesis_state"] = Json::Value(Json::objectValue);
  auto delegate_from = "0x7e4aa664f71de4e9d0b4a6473d796372639bdcde";
  auto &delegate_to = genesis_state[delegate_from] = Json::Value(Json::objectValue);
  delegate_to["0x780fe8b2226cf212c55635de399ee4c2a860810c"] = "0xf4240";
  delegate_to["0x56e0de6933d9d0453d0363caf42b136eb5854e4e"] = "0xf4240";
  delegate_to["0x71bdcbec7e3642782447b0fbf31eed068dfbdbb1"] = "0xf4240";

  auto &eth_chain_config = state["eth_chain_config"] = Json::Value(Json::objectValue);
  eth_chain_config["byzantium_block"] = "0x0";
  eth_chain_config["constantinople_block"] = "0x0";
  eth_chain_config["dao_fork_block"] = "0xffffffffffffffff";
  eth_chain_config["eip_150_block"] = "0x0";
  eth_chain_config["eip_158_block"] = "0x0";
  eth_chain_config["homestead_block"] = "0x0";
  eth_chain_config["petersburg_block"] = "0x0";

  auto &execution_options = state["execution_options"] = Json::Value(Json::objectValue);
  execution_options["disable_gas_fee"] = false;
  execution_options["disable_nonce_check"] = true;

  auto &genesis_balances = state["genesis_balances"] = Json::Value(Json::objectValue);
  auto addr1 = "2cd4da7d3b345e022ca7e997c2bb3276a4d3d2e9";
  auto addr2 = "7e4aa664f71de4e9d0b4a6473d796372639bdcde";
  genesis_balances[addr1] = "0x1027e72f1f12813088000000";
  genesis_balances[addr2] = "0x1027e72f1f12813088000000";

  auto &pbft = chain_config["pbft"] = Json::Value(Json::objectValue);
  pbft["committee_size"] = "0x3e8";
  pbft["dag_blocks_size"] = "0xa";
  pbft["ghost_path_move_back"] = "0x0";
  pbft["lambda_ms_min"] = "0x29a";
  pbft["run_count_votes"] = false;

  auto &vdf = chain_config["vdf"] = Json::Value(Json::objectValue);
  vdf["difficulty_max"] = "0x12";
  vdf["difficulty_min"] = "0x10";
  vdf["threshold_selection"] = "0xbffd";
  vdf["threshold_vdf_omit"] = "0x6bf7";
  vdf["difficulty_stale"] = "0x13";
  vdf["lambda_bound"] = "0x64";

  return chain_config;
}

Json::Value generateChainConfig(int network_identifier) {
  switch (network_identifier) {
    case Mainnet: {
      return mainnetChainConfig();
    }
    case Testnet: {
      return testnetChainConfig();
    }
    case Devnet: {
      return devnetChainConfig();
    }
    default: {
      stringstream err;
      err << "Wrong network identifier " << network_identifier << " (1=Mainnet, 2=Testnet, 3=Devnet)" << endl;
      throw invalid_argument(err.str());
    }
  }
}

Json::Value generateConfigFile(string &db_path, bool boot_node, int network_identifier) {
  Json::Value config_file(Json::objectValue);

  // Account
  auto account_file = getAccountFile();
  if (!fileExist(account_file)) {
    auto account = dev::KeyPair::create();
    auto account_json = getAccountJson(account);

    // Create account file
    writeFile(account_file, account_json);
  }

  // VRF
  auto vrf_file = getVrfFile();
  if (!fileExist(vrf_file)) {
    auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
    auto json = getVrfJson(sk, pk);

    // Create VRF file
    writeFile(vrf_file, json);
  }

  // DB
  if (db_path.empty()) {
    db_path = getTaraxaDefaultDir();
  }
  config_file["db_path"] = db_path;

  config_file["network_is_boot_node"] = boot_node;
  config_file["network_address"] = "0.0.0.0";
  config_file["network_tcp_port"] = 10002;
  config_file["network_udp_port"] = 10002;
  config_file["network_simulated_delay"] = 0;
  config_file["network_transaction_interval"] = 100;
  config_file["network_encrypted"] = 1;
  config_file["network_bandwidth"] = 40;
  config_file["network_ideal_peer_count"] = 10;
  config_file["network_max_peer_count"] = 50;
  config_file["network_sync_level_size"] = 10;
  config_file["network_boot_nodes"] = getNetworkBootNodes(network_identifier);

  auto &rpc = config_file["rpc"] = Json::Value(Json::objectValue);
  rpc["http_port"] = 7777;
  rpc["ws_port"] = 8777;
  rpc["threads_num"] = 10;

  auto &test_params = config_file["test_params"] = Json::Value(Json::objectValue);
  test_params["max_transaction_queue_warn"] = 0;
  test_params["max_transaction_queue_drop"] = 0;
  test_params["max_block_queue_warn"] = 0;
  test_params["db_snapshot_each_n_pbft_block"] = 100;
  test_params["db_max_snapshots"] = 5;
  auto &block_proposer = test_params["block_proposer"] = Json::Value(Json::objectValue);
  block_proposer["shard"] = 1;
  block_proposer["transaction_limit"] = 0;

  config_file["logging"] = generateLogging();

  config_file["chain_config"] = generateChainConfig(network_identifier);

  return config_file;
}

void nodeInfo() {
  cout << "NAME:\n\ttaraxad - Taraxa blockchain full node implementation" << endl;
  cout << "VERSION:\n\t" << TARAXAD_VERSION << endl;
  cout << "USAGE:\n\ttaraxad command [command options]\n" << endl;
}

void commandsInfo() {
  cout << "COMMANDS:" << endl;
  cout << "  node\t\t\t\tRuns the actual node (default)" << endl;
  cout << "  account\t\t\tGenerate new account private and public key" << endl;
  cout << "  account-from-key arg\t\tRestore account from private key" << endl;
  cout << "  vrf\t\t\t\tGenerate new VRF private and public key" << endl;
  cout << "  vrf-from-key arg\t\tRestore VRF from private key" << endl;
  cout << "  version\t\t\tPrints the taraxd version\n" << endl;
}

int main(int argc, const char *argv[]) {
  static_init();

  try {
    string config_file;
    string db_path;
    int network_identifier;
    bool boot_node = false;
    bool destroy_db;
    bool rebuild_db;
    uint64_t rebuild_db_period;
    bool rebuild_network;
    uint64_t revert_to_period;

    // Options
    bpo::options_description help_option("OPTIONS");
    help_option.add_options()("help", "Print this help message and exit");

    bpo::options_description node_options("NODE COMMAND OPTIONS");
    node_options.add_options()("config", bpo::value<string>(&config_file),
                               "JSON configuration file (default: \"~/.taraxa/config.json\")")(
        "datadir", bpo::value<string>(&db_path), "Data directory for the databases (default: \"~/.taraxa/db\")")(
        "networkId", bpo::value<int>(&network_identifier)->default_value(2),
        "Network identifier (integer, 1=Mainnet, 2=Testnet, 3=Devnet) (default: 2)")(
        "bootnode", bpo::bool_switch(&boot_node), "Run as bootnode (default: false)")(
        "destroy-db", bpo::bool_switch(&destroy_db)->default_value(false),
        "Destroys all the existing data in the database")(
        "rebuild-db", bpo::bool_switch(&rebuild_db)->default_value(false),
        "Reads the raw dag/pbft blocks from the db and executes all the blocks from scratch rebuilding all the other "
        "database tables - this could take a long time")("rebuild_db_period",
                                                         bpo::value<uint64_t>(&rebuild_db_period)->default_value(0),
                                                         "Use with rebuild-db - Rebuild db up to a specified period")(
        "rebuild-network", bpo::bool_switch(&rebuild_network)->default_value(false),
        "Delete all saved network/nodes information and rebuild network from boot nodes")(
        "revert-to-period", bpo::value<uint64_t>(&revert_to_period)->default_value(0),
        "Revert db/state to specified period (specify period)");

    bpo::options_description allowed_options;
    allowed_options.add(help_option);
    allowed_options.add(node_options);
    bpo::variables_map option_vars;
    auto parsed_line = bpo::parse_command_line(argc, argv, allowed_options);
    bpo::store(parsed_line, option_vars);
    bpo::notify(option_vars);

    if (option_vars.count("help")) {
      nodeInfo();
      cout << help_option << endl;
      commandsInfo();
      cout << node_options << endl;

      return 0;
    }

    // Commands
    bpo::options_description commands("COMMANDS");
    commands.add_options()("command", bpo::value<string>(), "command to execute")("subargs", bpo::value<string>(),
                                                                                  "Arguments for command");
    bpo::positional_options_description command_pos;
    command_pos.add("command", 1).add("subargs", 2);
    bpo::parsed_options command_parsed =
        bpo::command_line_parser(argc, argv).options(commands).positional(command_pos).allow_unregistered().run();
    bpo::variables_map command_vm;
    bpo::store(command_parsed, command_vm);
    if (!command_vm.empty()) {
      string cmd = command_vm["command"].as<string>();

      if (cmd == "account") {
        auto account_file = getAccountFile();
        if (fileExist(account_file)) {
          cout << "Account file exist at " << account_file << endl;
          return 0;
        }

        auto account = dev::KeyPair::create();
        auto json = getAccountJson(account);
        writeFile(account_file, json);

        cout << "Generate account file at " << account_file << endl;
        cout << "Please save the secret key at a safe place. Account secret key: " << toHex(account.secret().ref())
             << endl;
        cout << "Account public key: " << account.pub() << endl;
        cout << "Account address: " << account.address() << endl;

        return 0;
      }

      if (cmd == "account-from-key") {
        auto account_file = getAccountFile();
        if (fileExist(account_file)) {
          cout << "Account file exist at " << account_file << endl;
          return 0;
        }

        auto account_sk = command_vm["subargs"].as<string>();
        auto sk = dev::Secret(account_sk, dev::Secret::ConstructFromStringType::FromHex);
        auto account = dev::KeyPair(sk);
        auto json = getAccountJson(account);
        writeFile(account_file, json);

        cout << "Generate account file at " << account_file << endl;
        cout << "Please save the secret key at a safe place. Account secret key: " << toHex(account.secret().ref())
             << endl;
        cout << "Account public key: " << account.pub() << endl;
        cout << "Account address: " << account.address() << endl;

        return 0;
      }

      if (cmd == "vrf") {
        auto vrf_file = getVrfFile();
        if (fileExist(vrf_file)) {
          cout << "VRF file exist at " << vrf_file << endl;
          return 0;
        }

        auto [pk, sk] = taraxa::vrf_wrapper::getVrfKeyPair();
        auto json = getVrfJson(sk, pk);
        writeFile(vrf_file, json);

        cout << "Generate VRF file at " << vrf_file << endl;
        cout << "Please save the secret key at a safe place. VRF secret key: " << sk << endl;
        cout << "VRF public key: " << pk << endl;

        return 0;
      }

      if (cmd == "vrf-from-key") {
        auto vrf_file = getVrfFile();
        if (fileExist(vrf_file)) {
          cout << "VRF file exist at " << vrf_file << endl;
          return 0;
        }

        auto vrf_sk = command_vm["subargs"].as<string>();
        auto sk = taraxa::vrf_wrapper::vrf_sk_t(vrf_sk);
        auto pk = taraxa::vrf_wrapper::getVrfPublicKey(sk);
        auto json = getVrfJson(sk, pk);
        writeFile(vrf_file, json);

        cout << "Generate VRF file at " << vrf_file << endl;
        cout << "Please save the secret key at a safe place. VRF secret key: " << sk << endl;
        cout << "VRF public key: " << pk << endl;

        return 0;
      }

      if (cmd == "version") {
        cout << TARAXAD_VERSION << endl;
        return 0;
      }

      if (cmd != "node") {
        throw bpo::invalid_option_value(cmd);
      }
    }

    // node
    if (config_file.empty()) {
      config_file = getTaraxaDefaultDir() + "/config.json";
    }
    if (fileExist(config_file)) {
      cout << "Config file is exist at " << config_file << endl;
    } else {
      ofstream ofile(config_file);
      if (ofile.is_open()) {
        auto config = generateConfigFile(db_path, boot_node, network_identifier);
        ofile << config;
        ofile.close();
      } else {
        stringstream err;
        err << "Cannot open config file " << config_file << endl;
        throw invalid_argument(err.str());
      }
    }

    // Loads config
    FullNodeConfig cfg(config_file);

    // Validates config values
    if (!cfg.validate()) {
      cerr << "Invalid configration. Please make sure config values are valid";
      return 1;
    }

    cfg.node_secret = getAccountSecretKey().asString();
    cfg.vrf_secret = vrf_wrapper::vrf_sk_t(getVrfSecretKey().asString());
    cout << "Account secret key: " << cfg.node_secret << endl;
    cout << "VRF secret key: " << cfg.vrf_secret.toString() << endl;

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
  } catch (taraxa::ConfigException const &e) {
    cerr << "Configuration exception: " << e.what() << endl;
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << endl;
  }
  return 1;
}
