#pragma once
#include <libweb3jsonrpc/ModularServer.h>

#include "common/app_face.hpp"
#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "key_manager/key_manager.hpp"
#include "logger/logger.hpp"
#include "network/http_server.hpp"
#include "network/ws_server.hpp"

namespace taraxa {
namespace net {
class TaraxaFace;
class NetFace;
class EthFace;
class TestFace;
class DebugFace;
}  // namespace net

namespace metrics {
class MetricsService;
}

class App : public AppFace, std::enable_shared_from_this<App> {
 public:
  explicit App(FullNodeConfig const& conf);
  ~App();

  App(const App&) = delete;
  App(App&&) = delete;
  App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;
  void start();
  bool isStarted() const { return started_; }
  const FullNodeConfig& getConfig() const { return conf_; }
  std::shared_ptr<Network> getNetwork() const { return network_; }
  std::shared_ptr<TransactionManager> getTransactionManager() const { return trx_mgr_; }
  std::shared_ptr<DagManager> getDagManager() const { return dag_mgr_; }
  std::shared_ptr<DbStorage> getDB() const { return db_; }
  std::shared_ptr<PbftManager> getPbftManager() const { return pbft_mgr_; }
  std::shared_ptr<VoteManager> getVoteManager() const { return vote_mgr_; }
  std::shared_ptr<PbftChain> getPbftChain() const { return pbft_chain_; }
  std::shared_ptr<final_chain::FinalChain> getFinalChain() const { return final_chain_; }
  // used only in tests
  std::shared_ptr<DagBlockProposer> getDagBlockProposer() const { return dag_block_proposer_; }
  std::shared_ptr<GasPricer> getGasPricer() const { return gas_pricer_; }
  std::shared_ptr<pillar_chain::PillarChainManager> getPillarChainManager() const { return pillar_chain_mgr_; }

  const dev::Address& getAddress() const { return kp_.address(); }

  // For Debug
  uint64_t getProposedBlocksCount() const;

  void rebuildDb();

  void set_program_options(boost::program_options::options_description& command_line_options,
                           boost::program_options::options_description& configuration_file_options) const;
  void initialize(const fc::path& data_dir, std::shared_ptr<boost::program_options::variables_map> options) const;
  void startup();

  template <typename PluginType>
  std::shared_ptr<PluginType> register_plugin(bool auto_load = false) {
    auto plug = std::make_shared<PluginType>(*this);

    string cli_plugin_desc = plug->plugin_name() + " plugin. " + plug->plugin_description() + "\nOptions";
    boost::program_options::options_description plugin_cli_options(cli_plugin_desc);
    boost::program_options::options_description plugin_cfg_options;
    plug->plugin_set_program_options(plugin_cli_options, plugin_cfg_options);

    if (!plugin_cli_options.options().empty()) _cli_options.add(plugin_cli_options);

    if (!plugin_cfg_options.options().empty()) {
      std::string header_name = "plugin-cfg-header-" + plug->plugin_name();
      std::string header_desc = plug->plugin_name() + " plugin options";
      _cfg_options.add_options()(header_name.c_str(), header_desc.c_str());
      _cfg_options.add(plugin_cfg_options);
    }

    add_available_plugin(plug);

    if (auto_load) enable_plugin(plug->plugin_name());

    return plug;
  }
  std::shared_ptr<Plugin> get_plugin(const string& name) const;

  template <typename PluginType>
  std::shared_ptr<PluginType> get_plugin(const string& name) const {
    std::shared_ptr<Plugin> abs_plugin = get_plugin(name);
    std::shared_ptr<PluginType> result = std::dynamic_pointer_cast<PluginType>(abs_plugin);
    FC_ASSERT(result != std::shared_ptr<PluginType>(), "Unable to load plugin '${p}'", ("p", name));
    return result;
  }

  void enable_plugin(const string& name) const;

  bool is_plugin_enabled(const string& name) const;

  const string& get_node_info() const;

 private:
  using JsonRpcServer = ModularServer<net::TaraxaFace, net::NetFace, net::EthFace, net::TestFace, net::DebugFace>;

  // should be destroyed after all components, since they may depend on it through unsafe pointers
  std::unique_ptr<util::ThreadPool> rpc_thread_pool_;
  std::unique_ptr<util::ThreadPool> graphql_thread_pool_;

  // In case we will you config for this TP, it needs to be unique_ptr !!!
  util::ThreadPool subscription_pool_;

  std::atomic<bool> stopped_ = true;
  // configuration
  FullNodeConfig conf_;
  // Ethereum key pair
  dev::KeyPair kp_;

  // components
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<DbStorage> old_db_;
  std::shared_ptr<GasPricer> gas_pricer_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<Network> network_;
  std::shared_ptr<DagBlockProposer> dag_block_proposer_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_mgr_;
  std::shared_ptr<KeyManager> key_manager_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<net::HttpServer> jsonrpc_http_;
  std::shared_ptr<net::HttpServer> graphql_http_;
  std::shared_ptr<net::WsServer> jsonrpc_ws_;
  std::shared_ptr<net::WsServer> graphql_ws_;
  std::unique_ptr<JsonRpcServer> jsonrpc_api_;
  std::unique_ptr<metrics::MetricsService> metrics_;

  // logging
  LOG_OBJECTS_DEFINE

  std::atomic_bool started_ = 0;

  std::map<string, std::shared_ptr<Plugin>> _active_plugins;
  std::map<string, std::shared_ptr<Plugin>> _available_plugins;

  void init();
  void close();

  /**
   * @brief Method that is used to register metrics updaters.
   * So we don't need to pass metrics classes instances in other classes.
   */
  void setupMetricsUpdaters();
};

}  // namespace taraxa
