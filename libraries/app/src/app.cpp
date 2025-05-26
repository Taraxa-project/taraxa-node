#include "app/app.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <memory>

#include "common/config_exception.hpp"
#include "config/config_utils.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block.hpp"
#include "dag/dag_block_proposer.hpp"
#include "dag/dag_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "metrics/metrics_service.hpp"
#include "metrics/network_metrics.hpp"
#include "metrics/pbft_metrics.hpp"
#include "metrics/transaction_queue_metrics.hpp"
#include "pbft/pbft_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "slashing_manager/slashing_manager.hpp"
#include "storage/migration/migration_manager.hpp"
#include "transaction/gas_pricer.hpp"
#include "transaction/transaction_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

App::App() {}

App::~App() { close(); }

void App::addAvailablePlugin(std::shared_ptr<Plugin> plugin) { available_plugins_[plugin->name()] = plugin; }

void App::enablePlugin(const std::string &name) {
  if (available_plugins_[name] == nullptr) {
    throw std::runtime_error("Plugin " + name + " not found");
  }
  active_plugins_[name] = available_plugins_[name];
}

bool App::isPluginEnabled(const std::string &name) const { return active_plugins_.find(name) != active_plugins_.end(); }

void App::init(const cli::Config &cli_conf) {
  conf_ = cli_conf.getNodeConfiguration();

  fs::create_directories(conf_.db_path);
  fs::create_directories(conf_.log_path);

  // Initialize logging
  logger::Logging::get().Init();

  const auto &node_addr = conf_.getFirstWallet().node_addr;
  for (auto &logging : conf_.log_configs) {
    logging.InitLogging(node_addr);
  }

  // Note: call after InitLogging
  logger_ = logger::Logging::get().CreateChannelLogger("FULLND");

  std::string node_addresses;
  std::string node_public_keys;
  std::string node_vrf_public_keys;
  std::for_each(conf_.wallets.begin(), conf_.wallets.end(), [&](const WalletConfig &wallet) {
    node_addresses += wallet.node_addr.toString() + " ";
    node_public_keys += wallet.node_pk.toString() + " ";
    node_vrf_public_keys += wallet.vrf_pk.toString() + " ";
  });

  logger_->info("Node public keys: [{}]\nNode addresses: [{}]\nNode VRF public keys: [{}]", node_public_keys,
                node_addresses, node_vrf_public_keys);

  if (!conf_.genesis.dag_genesis_block.verifySig()) {
    logger_->error("Genesis block is invalid");
    assert(false);
  }
  {
    if (conf_.db_config.rebuild_db) {
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, true);
    }
    db_ = std::make_shared<DbStorage>(conf_.db_path,
                                      // Snapshots should be disabled while rebuilding
                                      conf_.db_config.rebuild_db ? 0 : conf_.db_config.db_snapshot_each_n_pbft_block,
                                      conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                      conf_.db_config.db_revert_to_period, false);

    if (db_->hasMajorVersionChanged()) {
      logger_->info("Major DB version has changed. Rebuilding Db");
      conf_.db_config.rebuild_db = true;
      db_ = nullptr;
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, true);
      db_ = std::make_shared<DbStorage>(conf_.db_path,
                                        0,  // Snapshots should be disabled while rebuilding
                                        conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                        conf_.db_config.db_revert_to_period);
    }

    db_->updateDbVersions();

    auto migration_manager = storage::migration::Manager(db_);
    migration_manager.applyAll();
    if (conf_.db_config.migrate_receipts_by_period) {
      migration_manager.applyReceiptsByPeriod();
    }
    if (db_->getDagBlocksCount() == 0) {
      db_->setGenesisHash(conf_.genesis.genesisHash());
    }
  }
  logger_->info("DB initialized ...");

  if (conf_.network.prometheus) {
    auto &config = *conf_.network.prometheus;
    logger_->info("Prometheus: server started at {}:{}. Polling interval is {} ms", config.address, config.listen_port,
                  config.polling_interval_ms);
    metrics_ =
        std::make_shared<metrics::MetricsService>(config.address, config.listen_port, config.polling_interval_ms);
  } else {
    logger_->info("Prometheus: config values aren't specified. Metrics collecting is disabled");
  }

  final_chain_ = std::make_shared<final_chain::FinalChain>(db_, conf_);
  key_manager_ = std::make_shared<KeyManager>(final_chain_);
  trx_mgr_ = std::make_shared<TransactionManager>(conf_, db_, final_chain_);
  gas_pricer_ = std::make_shared<GasPricer>(conf_.genesis, conf_.is_light_node, conf_.blocks_gas_pricer, trx_mgr_, db_);

  auto genesis_hash = conf_.genesis.genesisHash();
  auto genesis_hash_from_db = db_->getGenesisHash();
  if (!genesis_hash_from_db.has_value()) {
    logger_->error("Genesis hash was not found in DB. Something is wrong");
    std::terminate();
  }
  if (genesis_hash != genesis_hash_from_db) {
    logger_->error("Genesis hash {} is different with {} in DB", genesis_hash,
                   (genesis_hash_from_db.has_value() ? *genesis_hash_from_db : h256(0)));
    std::terminate();
  }

  pbft_chain_ = std::make_shared<PbftChain>(db_);
  dag_mgr_ = std::make_shared<DagManager>(conf_, trx_mgr_, pbft_chain_, final_chain_, db_, key_manager_);
  auto slashing_manager = std::make_shared<SlashingManager>(conf_, final_chain_, trx_mgr_, gas_pricer_);
  vote_mgr_ = std::make_shared<VoteManager>(conf_, db_, pbft_chain_, final_chain_, key_manager_, slashing_manager);
  pillar_chain_mgr_ = std::make_shared<pillar_chain::PillarChainManager>(conf_.genesis.state.hardforks.ficus_hf, db_,
                                                                         final_chain_, key_manager_);
  pbft_mgr_ = std::make_shared<PbftManager>(conf_, db_, pbft_chain_, vote_mgr_, dag_mgr_, trx_mgr_, final_chain_,
                                            pillar_chain_mgr_);
  dag_block_proposer_ = std::make_shared<DagBlockProposer>(conf_, dag_mgr_, trx_mgr_, final_chain_, db_, key_manager_);

  network_ = std::make_shared<Network>(conf_, genesis_hash, conf_.net_file_path().string(), db_, pbft_mgr_, pbft_chain_,
                                       vote_mgr_, dag_mgr_, trx_mgr_, std::move(slashing_manager), pillar_chain_mgr_);
  auto cli_options = cli_conf.getCliOptions();
  for (auto &plugin : active_plugins_) {
    plugin.second->init(cli_options);
  }
}

void App::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  scheduleLoggingConfigUpdate();

  if (!conf_.db_config.rebuild_db) {
    // GasPricer updater
    final_chain_->block_finalized_.subscribe(
        [gas_pricer = as_weak(gas_pricer_)](const auto &res) {
          if (auto gp = gas_pricer.lock()) {
            gp->update(res->trxs);
          }
        },
        subscription_pool_);

    final_chain_->block_finalized_.subscribe(
        [trx_manager = as_weak(trx_mgr_)](const auto &res) {
          if (auto trx_mgr = trx_manager.lock()) {
            trx_mgr->blockFinalized(res->final_chain_blk->number);
          }
        },
        subscription_pool_);
  }

  vote_mgr_->setNetwork(network_);
  pbft_mgr_->setNetwork(network_);
  dag_mgr_->setNetwork(network_);
  pillar_chain_mgr_->setNetwork(network_);

  if (conf_.db_config.rebuild_db) {
    rebuildDb();
    logger_->info("Rebuild db completed successfully. Restart node without db_rebuild option");
    started_ = false;
    return;
  } else if (conf_.db_config.migrate_only) {
    logger_->info("DB migrated successfully, please restart the node without the flag");
    started_ = false;
    return;
  } else {
    network_->start();
    dag_block_proposer_->setNetwork(network_);
    dag_block_proposer_->start();
  }

  pbft_mgr_->start();

  if (metrics_) {
    setupMetricsUpdaters();
    metrics_->start();
  }
  for (auto &plugin : active_plugins_) {
    logger_->info("Starting plugin {}", plugin.first);
    plugin.second->start();
  }
  started_ = true;
  logger_->info("Node started ... ");
}

void App::scheduleLoggingConfigUpdate() {
  // no file to check updates for (e.g. tests)
  if (conf_.json_file_name.empty()) {
    return;
  }

  config_update_executor_.post([&]() {
    while (started_ && !stopped_) {
      auto path = std::filesystem::path(conf_.json_file_name);
      if (path.empty()) {
        std::cout << "FullNodeConfig: scheduleLoggingConfigUpdate: json_file_name is empty" << std::endl;
        return;
      }
      auto update_time = std::filesystem::last_write_time(path);
      if (conf_.last_json_update_time >= update_time) {
        continue;
      }
      conf_.last_json_update_time = update_time;
      try {
        auto config = getJsonFromFileOrString(conf_.json_file_name);
        conf_.log_configs = conf_.loadLoggingConfigs(config["logging"]);
        conf_.InitLogging(conf_.getFirstWallet().node_addr);
      } catch (const ConfigException &e) {
        std::cerr << "FullNodeConfig: Failed to update logging config: " << e.what() << std::endl;
        continue;
      }
      std::cout << "FullNodeConfig: Updated logging config" << std::endl;
      std::this_thread::sleep_for(std::chrono::minutes(1));
    }
  });
}

void App::setupMetricsUpdaters() {
  auto network_metrics = metrics_->getMetrics<metrics::NetworkMetrics>();
  network_metrics->setPeersCountUpdater([network = network_]() { return network->getPeerCount(); });
  network_metrics->setDiscoveredPeersCountUpdater([network = network_]() { return network->getNodeCount(); });
  network_metrics->setSyncingDurationUpdater([network = network_]() { return network->syncTimeSeconds(); });

  auto transaction_queue_metrics = metrics_->getMetrics<metrics::TransactionQueueMetrics>();
  transaction_queue_metrics->setTransactionsCountUpdater(
      [trx_mgr = trx_mgr_]() { return trx_mgr->getTransactionPoolSize(); });
  transaction_queue_metrics->setGasPriceUpdater(
      [gas_pricer = gas_pricer_]() { return gas_pricer->bid().convert_to<double>(); });

  auto pbft_metrics = metrics_->getMetrics<metrics::PbftMetrics>();
  pbft_metrics->setPeriodUpdater([pbft_mgr = pbft_mgr_]() { return pbft_mgr->getPbftPeriod(); });
  pbft_metrics->setRoundUpdater([pbft_mgr = pbft_mgr_]() { return pbft_mgr->getPbftRound(); });
  pbft_metrics->setStepUpdater([pbft_mgr = pbft_mgr_]() { return pbft_mgr->getPbftStep(); });
  pbft_metrics->setVotesCountUpdater(
      [pbft_mgr = pbft_mgr_]() { return pbft_mgr->getCurrentNodeVotesCount().value_or(0); });
  final_chain_->block_finalized_.subscribe(
      [pbft_metrics](const std::shared_ptr<final_chain::FinalizationResult> &res) {
        pbft_metrics->setBlockNumber(res->final_chain_blk->number);
        pbft_metrics->setBlockTransactionsCount(res->trxs.size());
        pbft_metrics->setBlockTimestamp(res->final_chain_blk->timestamp);
      },
      subscription_pool_);
}

void App::close() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  dag_block_proposer_->stop();
  pbft_mgr_->stop();
  logger_->info("Node stopped ... ");
}

void App::rebuildDb() {
  pbft_mgr_->initialState();

  // Read pbft blocks one by one
  PbftPeriod period = 1;
  std::shared_ptr<PeriodData> period_data, next_period_data;
  std::atomic_bool stop_async = false;

  std::future<void> fut = std::async(std::launch::async, [this, &stop_async]() {
    while (!stop_async) {
      // While rebuilding pushSyncedPbftBlocksIntoChain will stay in its own internal loop
      pbft_mgr_->pushSyncedPbftBlocksIntoChain();
      thisThreadSleepForMilliSeconds(1);
    }
  });

  while (true) {
    std::vector<std::shared_ptr<PbftVote>> cert_votes;
    if (next_period_data != nullptr) {
      period_data = next_period_data;
    } else {
      auto data = old_db_->getPeriodDataRaw(period);
      if (data.size() == 0) break;
      period_data = std::make_shared<PeriodData>(std::move(data));
    }
    auto data = old_db_->getPeriodDataRaw(period + 1);
    if (data.size() == 0) {
      next_period_data = nullptr;
      // Latest finalized block cert votes are saved in db as 2t+1 cert votes
      auto votes = old_db_->getAllTwoTPlusOneVotes();
      for (auto v : votes) {
        if (v->getType() == PbftVoteTypes::cert_vote) cert_votes.push_back(v);
      }
    } else {
      next_period_data = std::make_shared<PeriodData>(std::move(data));
      // More efficient to get sender(which is expensive) on this thread which is not as busy as the thread that
      // pushes blocks to chain
      for (auto &t : next_period_data->transactions) t->getSender();
      cert_votes = next_period_data->previous_block_cert_votes;
    }

    logger_->info("Adding PBFT block {} from old DB into syncing queue for processing, final chain size: {}",
                  period_data->pbft_blk->getBlockHash().toString(), final_chain_->lastBlockNumber());

    pbft_mgr_->periodDataQueuePush(std::move(*period_data), dev::p2p::NodeID(), std::move(cert_votes));
    pbft_mgr_->waitForPeriodFinalization();
    period++;
    if (period % 100 == 0) {
      while (period - pbft_chain_->getPbftChainSize() > 100) {
        thisThreadSleepForMilliSeconds(1);
      }
    }

    if (period - 1 == conf_.db_config.rebuild_db_period) {
      break;
    }

    if (period % 10000 == 0) {
      logger_->info("Rebuilding period: {}", period);
    }
  }
  stop_async = true;
  fut.wait();
  // Handles the race case if some blocks are still in the queue
  pbft_mgr_->pushSyncedPbftBlocksIntoChain();
  logger_->info("Rebuild completed");
}

}  // namespace taraxa
