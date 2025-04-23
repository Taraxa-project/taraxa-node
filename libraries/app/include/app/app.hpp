#pragma once
#include <libweb3jsonrpc/ModularServer.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <memory>

#include "cli/config.hpp"
#include "common/app_base.hpp"
#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "key_manager/key_manager.hpp"
#include "logger/logger.hpp"
#include "plugin/plugin.hpp"

namespace taraxa {

class Plugin;

class App : public std::enable_shared_from_this<App>, public AppBase {
 public:
  App();
  virtual ~App();

  App(const App&) = delete;
  App(App&&) = delete;
  App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;
  void init(const cli::Config& cli_conf);
  void start();
  const FullNodeConfig& getConfig() const { return conf_; }
  std::shared_ptr<Network> getNetwork() const { return network_; }
  std::shared_ptr<TransactionManager> getTransactionManager() const { return trx_mgr_; }
  std::shared_ptr<DagManager> getDagManager() const { return dag_mgr_; }
  std::shared_ptr<DbStorage> getDB() const { return db_; }
  std::shared_ptr<PbftManager> getPbftManager() const { return pbft_mgr_; }
  std::shared_ptr<VoteManager> getVoteManager() const { return vote_mgr_; }
  std::shared_ptr<PbftChain> getPbftChain() const { return pbft_chain_; }
  std::shared_ptr<final_chain::FinalChain> getFinalChain() const { return final_chain_; }
  std::shared_ptr<metrics::MetricsService> getMetrics() const { return metrics_; }
  // used only in tests
  std::shared_ptr<DagBlockProposer> getDagBlockProposer() const { return dag_block_proposer_; }
  std::shared_ptr<GasPricer> getGasPricer() const { return gas_pricer_; }
  std::shared_ptr<pillar_chain::PillarChainManager> getPillarChainManager() const { return pillar_chain_mgr_; }
  // TODO[3023]
  const dev::Address& getAddress() const { return conf_.getFirstWallet().node_addr; }

  void rebuildDb();

  void initialize(const std::filesystem::path& data_dir,
                  std::shared_ptr<boost::program_options::variables_map> options) const;

  template <typename PluginType>
  std::shared_ptr<PluginType> registerPlugin(cli::Config& cli_conf) {
    auto plug = std::make_shared<PluginType>(shared_from_this());

    std::string cli_plugin_desc = plug->name() + " plugin. " + plug->description() + "\nOptions";
    boost::program_options::options_description plugin_cli_options(cli_plugin_desc);
    plug->addOptions(plugin_cli_options);
    if (!plugin_cli_options.options().empty()) {
      cli_conf.addCliOptions(plugin_cli_options);
    }

    addAvailablePlugin(plug);

    return plug;
  }

  std::string registeredPlugins() const {
    std::string plugins;
    return std::accumulate(available_plugins_.begin(), available_plugins_.end(), std::string(),
                           [](const auto& a, const auto& b) { return a.empty() ? b.first : a + " " + b.first; });
  }
  std::shared_ptr<Plugin> getPlugin(const std::string& name) const;

  template <typename PluginType>
  std::shared_ptr<PluginType> getPlugin(const std::string& name) const {
    std::shared_ptr<Plugin> abs_plugin = getPlugin(name);
    std::shared_ptr<PluginType> result = std::dynamic_pointer_cast<PluginType>(abs_plugin);

    return result;
  }
  void addAvailablePlugin(std::shared_ptr<Plugin> plugin);

  void enablePlugin(const std::string& name);

  bool isPluginEnabled(const std::string& name) const;

  void scheduleLoggingConfigUpdate();

 private:
  std::shared_ptr<util::ThreadPool> subscription_pool_ = std::make_shared<util::ThreadPool>(1);
  util::ThreadPool config_update_executor_{1};

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
  std::shared_ptr<metrics::MetricsService> metrics_;

  std::map<std::string, std::shared_ptr<Plugin>> active_plugins_;
  std::map<std::string, std::shared_ptr<Plugin>> available_plugins_;

  void close();

  /**
   * @brief Method that is used to register metrics updaters.
   * So we don't need to pass metrics classes instances in other classes.
   */
  void setupMetricsUpdaters();

  // logging
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa
