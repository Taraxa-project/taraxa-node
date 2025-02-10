#include "plugin/rpc.hpp"

#include <boost/program_options.hpp>

#include "graphql/http_processor.hpp"
#include "graphql/ws_server.hpp"
#include "network/rpc/Debug.h"
#include "network/rpc/Net.h"
#include "network/rpc/Taraxa.h"
#include "network/rpc/Test.h"
#include "network/rpc/eth/Eth.h"
#include "network/rpc/jsonrpc_http_processor.hpp"
#include "network/rpc/jsonrpc_ws_server.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::plugin {

namespace bpo = boost::program_options;
constexpr auto THREADS = "rpc.threads";
constexpr auto ENABLE_TEST_RPC = "rpc.enable-test-rpc";
constexpr auto ENABLE_DEBUG = "rpc.debug";

void Rpc::init(const boost::program_options::variables_map &opts) {
  if (!opts[THREADS].empty()) {
    threads_ = opts[THREADS].as<uint32_t>();
  }
  if (!opts[ENABLE_TEST_RPC].empty()) {
    enable_test_rpc_ = opts[ENABLE_TEST_RPC].as<bool>();
  }
  if (!opts[ENABLE_DEBUG].empty()) {
    enable_debug_ = opts[ENABLE_DEBUG].as<bool>();
  }
}

void Rpc::addOptions(boost::program_options::options_description &opts) {
  opts.add_options()(THREADS, bpo::value<uint32_t>(), "Number of threads for RPC");
  opts.add_options()(ENABLE_TEST_RPC, bpo::bool_switch()->default_value(false),
                     "Enables Test JsonRPC. Disabled by default");
  opts.add_options()(ENABLE_DEBUG, bpo::bool_switch()->default_value(false),
                     "Enables Debug RPC interface. Disabled by default");
}

void Rpc::start() {
  auto conf = app()->getConfig();
  if (threads_) {
    conf.network.rpc->threads_num = threads_;
  }
  // Inits rpc related members
  if (conf.network.rpc) {
    rpc_thread_pool_ = std::make_unique<util::ThreadPool>(conf.network.rpc->threads_num);
    net::rpc::eth::EthParams eth_rpc_params;
    eth_rpc_params.address = app()->getAddress();
    eth_rpc_params.chain_id = conf.genesis.chain_id;
    eth_rpc_params.gas_limit = conf.genesis.dag.gas_limit;
    eth_rpc_params.final_chain = app()->getFinalChain();
    eth_rpc_params.gas_pricer = [gas_pricer = app()->getGasPricer()]() { return gas_pricer->bid(); };
    eth_rpc_params.get_trx = [db = app()->getDB()](auto const &trx_hash) { return db->getTransaction(trx_hash); };
    eth_rpc_params.send_trx = [trx_manager = app()->getTransactionManager()](auto const &trx) {
      if (auto [ok, err_msg] = trx_manager->insertTransaction(trx); !ok) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error(fmt("Transaction is rejected.\n"
                                   "RLP: %s\n"
                                   "Reason: %s",
                                   dev::toJS(trx->rlp()), err_msg)));
      }
    };
    eth_rpc_params.syncing_probe = [network = app()->getNetwork(), pbft_chain = app()->getPbftChain(),
                                    pbft_mgr = app()->getPbftManager()] {
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
    if (enable_test_rpc_) {
      //  TODO Because this object refers to App, the lifecycle/dependency management is more complicated);
      test_json_rpc = std::make_shared<net::Test>(app());
    }

    std::shared_ptr<net::Debug> debug_json_rpc;
    if (enable_debug_) {
      // TODO Because this object refers to App, the lifecycle/dependency management is more complicated);
      debug_json_rpc = std::make_shared<net::Debug>(app(), conf.genesis.dag.gas_limit);
    }

    jsonrpc_api_ = std::make_unique<JsonRpcServer>(
        std::make_shared<net::Taraxa>(app()),  // TODO Because this object refers to App, the
                                               // lifecycle/dependency management is more complicated
        std::make_shared<net::Net>(app()),     // TODO Because this object refers to App, the
                                               // lifecycle/dependency management is more complicated
        eth_json_rpc, test_json_rpc, debug_json_rpc);

    if (conf.network.rpc->http_port) {
      auto json_rpc_processor = std::make_shared<net::JsonRpcHttpProcessor>();
      jsonrpc_http_ = std::make_shared<net::HttpServer>(
          rpc_thread_pool_, boost::asio::ip::tcp::endpoint{conf.network.rpc->address, *conf.network.rpc->http_port},
          app()->getAddress(), json_rpc_processor, conf.network.rpc->max_pending_tasks);
      jsonrpc_api_->addConnector(json_rpc_processor);
      jsonrpc_http_->start();
    }
    if (conf.network.rpc->ws_port) {
      jsonrpc_ws_ = std::make_shared<net::JsonRpcWsServer>(
          rpc_thread_pool_, boost::asio::ip::tcp::endpoint{conf.network.rpc->address, *conf.network.rpc->ws_port},
          app()->getAddress(), conf.network.rpc->max_pending_tasks);
      jsonrpc_api_->addConnector(jsonrpc_ws_);
      jsonrpc_ws_->run();
    }
    if (!conf.db_config.rebuild_db) {
      app()->getFinalChain()->block_finalized_.subscribe(
          [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_),
           db = as_weak(app()->getDB())](const auto &res) {
            if (auto _eth_json_rpc = eth_json_rpc.lock()) {
              _eth_json_rpc->note_block_executed(*res->final_chain_blk, res->trxs, res->trx_receipts);
            }
            if (auto _ws = ws.lock()) {
              if (_ws->numberOfSessions()) {
                _ws->newEthBlock(*res->final_chain_blk, hashes_from_transactions(res->trxs));
                if (auto _db = db.lock()) {
                  auto pbft_blk = _db->getPbftBlock(res->hash);
                  if (const auto &hash = pbft_blk->getPivotDagBlockHash(); hash != kNullBlockHash) {
                    _ws->newDagBlockFinalized(hash, pbft_blk->getPeriod());
                  }
                  _ws->newPbftBlockExecuted(*pbft_blk, res->dag_blk_hashes);
                }
              }
            }
          },
          rpc_thread_pool_);
    }

    app()->getTransactionManager()->transaction_added_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_)](const auto &trx_hash) {
          if (auto _eth_json_rpc = eth_json_rpc.lock()) {
            _eth_json_rpc->note_pending_transaction(trx_hash);
          }
          if (auto _ws = ws.lock()) {
            _ws->newPendingTransaction(trx_hash);
          }
        },
        rpc_thread_pool_);
    app()->getDagManager()->block_verified_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_)](const auto &dag_block) {
          if (auto _ws = ws.lock()) {
            _ws->newDagBlock(dag_block);
          }
        },
        rpc_thread_pool_);

    app()->getPillarChainManager()->pillar_block_finalized_.subscribe(
        [ws_weak = as_weak(jsonrpc_ws_)](const auto &pillar_block_data) {
          if (auto ws = ws_weak.lock()) {
            ws->newPillarBlockData(pillar_block_data);
          }
        },
        rpc_thread_pool_);
  }
  if (conf.network.graphql) {
    graphql_thread_pool_ = std::make_shared<util::ThreadPool>(conf.network.graphql->threads_num);
    if (conf.network.graphql->ws_port) {
      graphql_ws_ = std::make_shared<net::GraphQlWsServer>(
          graphql_thread_pool_,
          boost::asio::ip::tcp::endpoint{conf.network.graphql->address, *conf.network.graphql->ws_port},
          app()->getAddress(), conf.network.rpc->max_pending_tasks);
      // graphql_ws_->run();
    }

    if (conf.network.graphql->http_port) {
      graphql_http_ = std::make_shared<net::HttpServer>(
          graphql_thread_pool_,
          boost::asio::ip::tcp::endpoint{conf.network.graphql->address, *conf.network.graphql->http_port},
          app()->getAddress(),
          std::make_shared<net::GraphQlHttpProcessor>(
              app()->getFinalChain(), app()->getDagManager(), app()->getPbftManager(), app()->getTransactionManager(),
              app()->getDB(), app()->getGasPricer(), as_weak(app()->getNetwork()), conf.genesis.chain_id),
          conf.network.rpc->max_pending_tasks);
      graphql_http_->start();
    }
  }
}

void Rpc::shutdown() {
  jsonrpc_api_ = nullptr;  // TODO remove this line - we should not care about destroying objects explicitly, the
                           // lifecycle of objects should be as declarative as possible (RAII).
                           // This line is needed because jsonrpc_api_ indirectly refers to App (produces
                           // self-reference from App to App).
}

}  // namespace taraxa::plugin