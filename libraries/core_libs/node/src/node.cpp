#include "node/node.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <stdexcept>

#include "dag/dag.hpp"
#include "dag/dag_block.hpp"
#include "dag/dag_block_proposer.hpp"
#include "graphql/http_processor.hpp"
#include "graphql/ws_server.hpp"
#include "key_manager/key_manager.hpp"
#include "metrics/metrics_service.hpp"
#include "metrics/network_metrics.hpp"
#include "metrics/pbft_metrics.hpp"
#include "metrics/transaction_queue_metrics.hpp"
#include "network/rpc/Debug.h"
#include "network/rpc/Net.h"
#include "network/rpc/Taraxa.h"
#include "network/rpc/Test.h"
#include "network/rpc/eth/Eth.h"
#include "network/rpc/jsonrpc_http_processor.hpp"
#include "network/rpc/jsonrpc_ws_server.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/migration/migration_manager.hpp"
#include "transaction/gas_pricer.hpp"
#include "transaction/transaction_manager.hpp"

namespace taraxa {

FullNode::FullNode(FullNodeConfig const &conf) : subscription_pool_(1), conf_(conf), kp_(conf_.node_secret) { init(); }

FullNode::~FullNode() { close(); }

void FullNode::init() {
  fs::create_directories(conf_.db_path);
  fs::create_directories(conf_.log_path);
  // Initialize logging
  auto const &node_addr = kp_.address();

  for (auto &logging : conf_.log_configs) {
    logging.InitLogging(node_addr);
  }

  LOG_OBJECTS_CREATE("FULLND");

  LOG(log_si_) << "Node public key: " << EthGreen << kp_.pub().toString() << std::endl
               << EthReset << "Node address: " << EthRed << node_addr.toString() << std::endl
               << EthReset << "Node VRF public key: " << EthGreen
               << vrf_wrapper::getVrfPublicKey(conf_.vrf_secret).toString() << EthReset;

  if (!conf_.genesis.dag_genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  {
    if (conf_.db_config.rebuild_db) {
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, node_addr, true);
    }
    db_ = std::make_shared<DbStorage>(conf_.db_path,
                                      // Snapshots should be disabled while rebuilding
                                      conf_.db_config.rebuild_db ? 0 : conf_.db_config.db_snapshot_each_n_pbft_block,
                                      conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                      conf_.db_config.db_revert_to_period, node_addr, false);

    if (db_->hasMajorVersionChanged()) {
      LOG(log_si_) << "Major DB version has changed. Rebuilding Db";
      conf_.db_config.rebuild_db = true;
      db_ = nullptr;
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, node_addr, true);
      db_ = std::make_shared<DbStorage>(conf_.db_path,
                                        0,  // Snapshots should be disabled while rebuilding
                                        conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                        conf_.db_config.db_revert_to_period, node_addr);
    } else if (db_->hasMinorVersionChanged()) {
      storage::migration::Manager(db_).applyAll();
    }
    db_->updateDbVersions();

    if (db_->getDagBlocksCount() == 0) {
      db_->setGenesisHash(conf_.genesis.genesisHash());
    }
  }
  LOG(log_nf_) << "DB initialized ...";

  if (conf_.network.prometheus) {
    auto &config = *conf_.network.prometheus;
    LOG(log_nf_) << "Prometheus: server started at " << config.address << ":" << config.listen_port
                 << ". Polling interval is " << config.polling_interval_ms << "ms";
    metrics_ =
        std::make_unique<metrics::MetricsService>(config.address, config.listen_port, config.polling_interval_ms);
  } else {
    LOG(log_nf_) << "Prometheus: config values aren't specified. Metrics collecting is disabled";
  }

  gas_pricer_ = std::make_shared<GasPricer>(conf_.genesis.gas_price, conf_.is_light_node, db_);
  final_chain_ = NewFinalChain(db_, conf_, node_addr);
  key_manager_ = std::make_shared<KeyManager>(final_chain_);
  trx_mgr_ = std::make_shared<TransactionManager>(conf_, db_, final_chain_, node_addr);

  auto genesis_hash = conf_.genesis.genesisHash();
  auto genesis_hash_from_db = db_->getGenesisHash();
  if (!genesis_hash_from_db.has_value()) {
    LOG(log_er_) << "Genesis hash was not found in DB. Something is wrong";
    assert(false);
  }
  if (genesis_hash != genesis_hash_from_db) {
    LOG(log_er_) << "Genesis hash " << genesis_hash << " is different with "
                 << (genesis_hash_from_db.has_value() ? *genesis_hash_from_db : h256(0)) << " in DB";
    assert(false);
  }

  pbft_chain_ = std::make_shared<PbftChain>(node_addr, db_);
  dag_mgr_ = std::make_shared<DagManager>(conf_.genesis.dag_genesis_block, node_addr, conf_.genesis.sortition,
                                          conf_.genesis.dag, trx_mgr_, pbft_chain_, final_chain_, db_, key_manager_,
                                          conf_.genesis.pbft.gas_limit, conf_.is_light_node, conf_.light_node_history,
                                          conf_.max_levels_per_period, conf_.dag_expiry_limit);
  vote_mgr_ = std::make_shared<VoteManager>(node_addr, conf_.genesis.pbft, kp_.secret(), conf_.vrf_secret, db_,
                                            pbft_chain_, final_chain_, key_manager_);
  pbft_mgr_ =
      std::make_shared<PbftManager>(conf_.genesis.pbft, conf_.genesis.dag_genesis_block.getHash(), node_addr, db_,
                                    pbft_chain_, vote_mgr_, dag_mgr_, trx_mgr_, final_chain_, kp_.secret());
  dag_block_proposer_ = std::make_shared<DagBlockProposer>(
      conf_.genesis.dag.block_proposer, dag_mgr_, trx_mgr_, final_chain_, db_, key_manager_, node_addr, getSecretKey(),
      getVrfSecretKey(), conf_.genesis.pbft.gas_limit, conf_.genesis.dag.gas_limit);
  network_ = std::make_shared<Network>(conf_, genesis_hash, conf_.net_file_path().string(), kp_, db_, pbft_mgr_,
                                       pbft_chain_, vote_mgr_, dag_mgr_, trx_mgr_);
}

void FullNode::setupMetricsUpdaters() {
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
  final_chain_->block_finalized_.subscribe([pbft_metrics](const std::shared_ptr<final_chain::FinalizationResult> &res) {
    pbft_metrics->setBlockNumber(res->final_chain_blk->number);
    pbft_metrics->setBlockTransactionsCount(res->trxs.size());
    pbft_metrics->setBlockTimestamp(res->final_chain_blk->timestamp);
  });
}

void FullNode::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  // Inits rpc related members
  if (conf_.network.rpc) {
    rpc_thread_pool_ = std::make_unique<util::ThreadPool>(conf_.network.rpc->threads_num);
    net::rpc::eth::EthParams eth_rpc_params;
    eth_rpc_params.address = getAddress();
    eth_rpc_params.chain_id = conf_.genesis.chain_id;
    eth_rpc_params.gas_limit = conf_.genesis.dag.gas_limit;
    eth_rpc_params.final_chain = final_chain_;
    eth_rpc_params.gas_pricer = [gas_pricer = gas_pricer_]() { return gas_pricer->bid(); };
    eth_rpc_params.get_trx = [db = db_](auto const &trx_hash) { return db->getTransaction(trx_hash); };
    eth_rpc_params.send_trx = [trx_manager = trx_mgr_](auto const &trx) {
      if (auto [ok, err_msg] = trx_manager->insertTransaction(trx); !ok) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error(fmt("Transaction is rejected.\n"
                                   "RLP: %s\n"
                                   "Reason: %s",
                                   dev::toJS(trx->rlp()), err_msg)));
      }
    };
    eth_rpc_params.syncing_probe = [network = network_, pbft_chain = pbft_chain_, pbft_mgr = pbft_mgr_] {
      std::optional<net::rpc::eth::SyncStatus> ret;
      if (!network->pbft_syncing()) {
        return ret;
      }
      auto &status = ret.emplace();
      // TODO clearly define Ethereum json-rpc "syncing" in Taraxa
      status.current_block = pbft_chain->getPbftChainSize();
      status.starting_block = status.current_block;
      status.highest_block = pbft_mgr->pbftSyncingPeriod();
      return ret;
    };

    auto eth_json_rpc = net::rpc::eth::NewEth(std::move(eth_rpc_params));
    std::shared_ptr<net::Test> test_json_rpc;
    if (conf_.enable_test_rpc) {
      // TODO Because this object refers to FullNode, the lifecycle/dependency management is more complicated);
      test_json_rpc = std::make_shared<net::Test>(shared_from_this());
    }

    std::shared_ptr<net::Debug> debug_json_rpc;
    if (conf_.enable_debug) {
      // TODO Because this object refers to FullNode, the lifecycle/dependency management is more complicated);
      debug_json_rpc = std::make_shared<net::Debug>(shared_from_this(), conf_.genesis.dag.gas_limit);
    }

    jsonrpc_api_ = std::make_unique<jsonrpc_server_t>(
        std::make_shared<net::Taraxa>(shared_from_this()),  // TODO Because this object refers to FullNode, the
                                                            // lifecycle/dependency management is more complicated
        std::make_shared<net::Net>(shared_from_this()),     // TODO Because this object refers to FullNode, the
                                                            // lifecycle/dependency management is more complicated
        eth_json_rpc, test_json_rpc, debug_json_rpc);

    if (conf_.network.rpc->http_port) {
      auto json_rpc_processor = std::make_shared<net::JsonRpcHttpProcessor>();
      jsonrpc_http_ = std::make_shared<net::HttpServer>(
          rpc_thread_pool_->unsafe_get_io_context(),
          boost::asio::ip::tcp::endpoint{conf_.network.rpc->address, *conf_.network.rpc->http_port}, getAddress(),
          json_rpc_processor);
      jsonrpc_api_->addConnector(json_rpc_processor);
      jsonrpc_http_->start();
    }
    if (conf_.network.rpc->ws_port) {
      jsonrpc_ws_ = std::make_shared<net::JsonRpcWsServer>(
          rpc_thread_pool_->unsafe_get_io_context(),
          boost::asio::ip::tcp::endpoint{conf_.network.rpc->address, *conf_.network.rpc->ws_port}, getAddress());
      jsonrpc_api_->addConnector(jsonrpc_ws_);
      jsonrpc_ws_->run();
    }
    final_chain_->block_finalized_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_), db = as_weak(db_)](auto const &res) {
          if (auto _eth_json_rpc = eth_json_rpc.lock()) {
            _eth_json_rpc->note_block_executed(*res->final_chain_blk, res->trxs, res->trx_receipts);
          }
          if (auto _ws = ws.lock()) {
            _ws->newEthBlock(*res->final_chain_blk, hashes_from_transactions(res->trxs));
            if (auto _db = db.lock()) {
              auto pbft_blk = _db->getPbftBlock(res->hash);
              if (const auto &hash = pbft_blk->getPivotDagBlockHash(); hash != kNullBlockHash) {
                _ws->newDagBlockFinalized(hash, pbft_blk->getPeriod());
              }
              _ws->newPbftBlockExecuted(*pbft_blk, res->dag_blk_hashes);
            }
          }
        },
        *rpc_thread_pool_);

    trx_mgr_->transaction_accepted_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_)](auto const &trx_hash) {
          if (auto _eth_json_rpc = eth_json_rpc.lock()) {
            _eth_json_rpc->note_pending_transaction(trx_hash);
          }
          if (auto _ws = ws.lock()) {
            _ws->newPendingTransaction(trx_hash);
          }
        },
        *rpc_thread_pool_);
    dag_mgr_->block_verified_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_)](auto const &dag_block) {
          if (auto _ws = ws.lock()) {
            _ws->newDagBlock(dag_block);
          }
        },
        *rpc_thread_pool_);
  }
  if (conf_.network.graphql) {
    graphql_thread_pool_ = std::make_unique<util::ThreadPool>(conf_.network.graphql->threads_num);
    if (conf_.network.graphql->ws_port) {
      graphql_ws_ = std::make_shared<net::GraphQlWsServer>(
          graphql_thread_pool_->unsafe_get_io_context(),
          boost::asio::ip::tcp::endpoint{conf_.network.graphql->address, *conf_.network.graphql->ws_port},
          getAddress());
      // graphql_ws_->run();
    }

    if (conf_.network.graphql->http_port) {
      graphql_http_ = std::make_shared<net::HttpServer>(
          graphql_thread_pool_->unsafe_get_io_context(),
          boost::asio::ip::tcp::endpoint{conf_.network.graphql->address, *conf_.network.graphql->http_port},
          getAddress(),
          std::make_shared<net::GraphQlHttpProcessor>(final_chain_, dag_mgr_, pbft_mgr_, trx_mgr_, db_, gas_pricer_,
                                                      as_weak(network_), conf_.genesis.chain_id));
      graphql_http_->start();
    }
  }

  // GasPricer updater
  final_chain_->block_finalized_.subscribe(
      [gas_pricer = as_weak(gas_pricer_)](auto const &res) {
        if (auto gp = gas_pricer.lock()) {
          gp->update(res->trxs);
        }
      },
      subscription_pool_);

  final_chain_->block_finalized_.subscribe(
      [trx_manager = as_weak(trx_mgr_)](auto const &res) {
        if (auto trx_mgr = trx_manager.lock()) {
          trx_mgr->blockFinalized(res->final_chain_blk->number);
        }
      },
      subscription_pool_);

  vote_mgr_->setNetwork(network_);
  pbft_mgr_->setNetwork(network_);
  dag_mgr_->setNetwork(network_);

  if (conf_.db_config.rebuild_db) {
    rebuildDb();
    LOG(log_si_) << "Rebuild db completed successfully. Restart node without db_rebuild option";
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
  started_ = true;
  LOG(log_nf_) << "Node started ... ";
}

void FullNode::close() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  jsonrpc_api_ = nullptr;  // TODO remove this line - we should not care about destroying objects explicitly, the
                           // lifecycle of objects should be as declarative as possible (RAII).
                           // This line is needed because jsonrpc_api_ indirectly refers to FullNode (produces
                           // self-reference from FullNode to FullNode).

  dag_block_proposer_->stop();
  pbft_mgr_->stop();
  LOG(log_nf_) << "Node stopped ... ";
}

void FullNode::rebuildDb() {
  pbft_mgr_->initialState();

  // Read pbft blocks one by one
  PbftPeriod period = 1;
  std::shared_ptr<PeriodData> period_data, next_period_data;
  while (true) {
    std::vector<std::shared_ptr<Vote>> cert_votes;
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
      cert_votes = next_period_data->previous_block_cert_votes;
    }

    LOG(log_nf_) << "Adding PBFT block " << period_data->pbft_blk->getBlockHash().toString()
                 << " from old DB into syncing queue for processing";
    pbft_mgr_->addRebuildDBPeriodData(std::move(*period_data), std::move(cert_votes));
    period++;

    if (period - 1 == conf_.db_config.rebuild_db_period) {
      break;
    }
  }

  while (final_chain_->last_block_number() != period - 1) {
    thisThreadSleepForMilliSeconds(1000);
    LOG(log_nf_) << "Waiting on PBFT blocks to be processed. PBFT chain size " << pbft_mgr_->pbftSyncingPeriod()
                 << ", final chain size: " << final_chain_->last_block_number();
  }
}

uint64_t FullNode::getProposedBlocksCount() const { return dag_block_proposer_->getProposedBlocksCount(); }

}  // namespace taraxa
