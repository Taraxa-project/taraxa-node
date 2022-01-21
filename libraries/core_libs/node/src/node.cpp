#include "node/node.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <stdexcept>

#include "dag/dag.hpp"
#include "dag/dag_block.hpp"
#include "network/rpc/Net.h"
#include "network/rpc/Taraxa.h"
#include "network/rpc/Test.h"
#include "network/rpc/eth/Eth.h"
#include "network/rpc/rpc_error_handler.hpp"
#include "pbft/block_proposer.hpp"
#include "pbft/pbft_manager.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa {

FullNode::FullNode(FullNodeConfig const &conf)
    : conf_(conf),
      kp_(conf_.node_secret.empty()
              ? dev::KeyPair::create()
              : dev::KeyPair(dev::Secret(conf_.node_secret, dev::Secret::ConstructFromStringType::FromHex))) {
  init();
}

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
  log_time_ = logger::createLogger(logger::Verbosity::Info, "TMSTM", node_addr);

  LOG(log_si_) << "Node public key: " << EthGreen << kp_.pub().toString() << std::endl
               << EthReset << "Node address: " << EthRed << node_addr.toString() << std::endl
               << EthReset << "Node VRF public key: " << EthGreen
               << vrf_wrapper::getVrfPublicKey(conf_.vrf_secret).toString() << EthReset;

  if (!conf_.chain.dag_genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  {
    if (conf_.test_params.rebuild_db) {
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                                            conf_.test_params.db_max_open_files, conf_.test_params.db_max_snapshots,
                                            conf_.test_params.db_revert_to_period, node_addr, true);
    }

    db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                                      conf_.test_params.db_max_open_files, conf_.test_params.db_max_snapshots,
                                      conf_.test_params.db_revert_to_period, node_addr, false,
                                      conf_.test_params.rebuild_db_columns);

    if (db_->hasMinorVersionChanged()) {
      LOG(log_si_) << "Minor DB version has changed. Rebuilding Db";
      conf_.test_params.rebuild_db = true;
      db_ = nullptr;
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                                            conf_.test_params.db_max_open_files, conf_.test_params.db_max_snapshots,
                                            conf_.test_params.db_revert_to_period, node_addr, true);
      db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                                        conf_.test_params.db_max_open_files, conf_.test_params.db_max_snapshots,
                                        conf_.test_params.db_revert_to_period, node_addr);
    }

    if (db_->getNumDagBlocks() == 0) {
      db_->saveDagBlock(conf_.chain.dag_genesis_block);
    }
  }
  LOG(log_nf_) << "DB initialized ...";

  final_chain_ = NewFinalChain(db_, conf_.chain.final_chain, node_addr);
  trx_mgr_ = std::make_shared<TransactionManager>(conf_, node_addr, db_);

  auto genesis_hash = conf_.chain.dag_genesis_block.getHash();
  auto dag_genesis_hash_from_db = *db_->getBlocksByLevel(0).begin();
  if (genesis_hash != dag_genesis_hash_from_db) {
    LOG(log_er_) << "The DAG genesis block hash " << genesis_hash << " in config is different with "
                 << dag_genesis_hash_from_db << " in DB";
    assert(false);
  }

  pbft_chain_ = std::make_shared<PbftChain>(genesis_hash, node_addr, db_);
  next_votes_mgr_ = std::make_shared<NextVotesManager>(node_addr, db_, final_chain_);
  dag_blk_mgr_ = std::make_shared<DagBlockManager>(node_addr, conf_.chain.sortition, conf_.chain.final_chain.state.dpos,
                                                   4 /* verifer thread*/, db_, trx_mgr_, final_chain_, pbft_chain_,
                                                   log_time_, conf_.test_params.max_block_queue_warn);
  dag_mgr_ = std::make_shared<DagManager>(genesis_hash, node_addr, trx_mgr_, pbft_chain_, dag_blk_mgr_, db_, log_time_);
  vote_mgr_ = std::make_shared<VoteManager>(node_addr, db_, final_chain_, pbft_chain_, next_votes_mgr_);

  timing_machine_ = std::make_shared<TimingMachine>(node_addr);
  pbft_mgr_ = std::make_shared<PbftManager>(node_addr, conf_.chain.pbft, genesis_hash, kp_.secret(), conf_.vrf_secret,
                                            db_, pbft_chain_, next_votes_mgr_, vote_mgr_, dag_mgr_, dag_blk_mgr_,
                                            trx_mgr_, final_chain_, timing_machine_);
  // TODO: need remove
  vote_mgr_->setPbftManager(pbft_mgr_);

  blk_proposer_ =
      std::make_shared<BlockProposer>(conf_.test_params.block_proposer, dag_mgr_, trx_mgr_, dag_blk_mgr_, final_chain_,
                                      node_addr, getSecretKey(), getVrfSecretKey(), log_time_);
  network_ = std::make_shared<Network>(conf_.network, conf_.net_file_path().string(), kp_, db_, pbft_mgr_, pbft_chain_,
                                       vote_mgr_, next_votes_mgr_, dag_mgr_, dag_blk_mgr_, trx_mgr_);
}

void FullNode::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  // Inits rpc related members
  if (conf_.rpc) {
    rpc_thread_pool_ = std::make_unique<util::ThreadPool>(conf_.rpc->threads_num);
    net::rpc::eth::EthParams eth_rpc_params;
    eth_rpc_params.address = getAddress();
    eth_rpc_params.secret = kp_.secret();
    eth_rpc_params.chain_id = conf_.chain.chain_id;
    eth_rpc_params.final_chain = final_chain_;
    eth_rpc_params.get_trx = [db = db_](auto const &trx_hash) { return db->getTransaction(trx_hash); };
    eth_rpc_params.send_trx = [trx_manager = trx_mgr_](auto const &trx) {
      if (auto [ok, err_msg] = trx_manager->insertTransaction(trx); !ok) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error(fmt("Transaction is rejected.\n"
                                   "RLP: %s\n"
                                   "Reason: %s",
                                   dev::toJS(*trx.rlp()), err_msg)));
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
    jsonrpc_api_ = std::make_unique<jsonrpc_server_t>(
        make_shared<net::Test>(shared_from_this()),    // TODO Because this object refers to FullNode, the
                                                       // lifecycle/dependency management is more complicated
        make_shared<net::Taraxa>(shared_from_this()),  // TODO Because this object refers to FullNode, the
                                                       // lifecycle/dependency management is more complicated
        make_shared<net::Net>(shared_from_this()),     // TODO Because this object refers to FullNode, the
                                                       // lifecycle/dependency management is more complicated
        eth_json_rpc);
    if (conf_.rpc->http_port) {
      jsonrpc_http_ =
          std::make_shared<net::RpcServer>(rpc_thread_pool_->unsafe_get_io_context(),
                                           boost::asio::ip::tcp::endpoint{conf_.rpc->address, *conf_.rpc->http_port},
                                           getAddress(), net::handle_rpc_error);
      jsonrpc_api_->addConnector(jsonrpc_http_);
      jsonrpc_http_->StartListening();
    }
    if (conf_.rpc->ws_port) {
      jsonrpc_ws_ = std::make_shared<net::WSServer>(
          rpc_thread_pool_->unsafe_get_io_context(),
          boost::asio::ip::tcp::endpoint{conf_.rpc->address, *conf_.rpc->ws_port}, getAddress());
      jsonrpc_api_->addConnector(jsonrpc_ws_);
      jsonrpc_ws_->run();
    }
    final_chain_->block_finalized_.subscribe(
        [eth_json_rpc = as_weak(eth_json_rpc), ws = as_weak(jsonrpc_ws_), db = as_weak(db_)](auto const &res) {
          if (auto _eth_json_rpc = eth_json_rpc.lock()) {
            _eth_json_rpc->note_block_executed(*res->final_chain_blk, res->trxs, res->trx_receipts);
          }
          if (auto _ws = ws.lock()) {
            _ws->newEthBlock(*res->final_chain_blk);
            if (auto _db = db.lock()) {
              auto pbft_blk = _db->getPbftBlock(res->hash);
              _ws->newDagBlockFinalized(pbft_blk->getPivotDagBlockHash(), pbft_blk->getPeriod());
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

  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;

  if (conf_.network.network_is_boot_node) {
    LOG(log_nf_) << "Starting a boot node ..." << std::endl;
  }
  if (!conf_.test_params.rebuild_db) {
    network_->start();
    blk_proposer_->setNetwork(network_);
    blk_proposer_->start();
  }
  vote_mgr_->setNetwork(network_);
  pbft_mgr_->setNetwork(network_);
  dag_mgr_->setNetwork(network_);
  pbft_mgr_->start();
  dag_blk_mgr_->start();
  dag_mgr_->start();

  if (conf_.test_params.rebuild_db) {
    rebuildDb();
    LOG(log_si_) << "Rebuild db completed successfully. Restart node without db_rebuild option";
    started_ = false;
    return;
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
  blk_proposer_->stop();
  pbft_mgr_->stop();
  dag_blk_mgr_->stop();
  dag_mgr_->stop();
  LOG(log_nf_) << "Node stopped ... ";
}

void FullNode::rebuildDb() {
  // Read pbft blocks one by one
  uint64_t period = 1;

  while (true) {
    auto data = old_db_->getPeriodDataRaw(period);
    if (data.size() == 0) {
      break;
    }
    SyncBlock sync_block(data);

    LOG(log_nf_) << "Adding sync block into queue " << sync_block.pbft_blk->getBlockHash().toString();
    pbft_mgr_->syncBlockQueuePush(std::move(sync_block), dev::p2p::NodeID());

    // Wait if more than 10 pbft blocks in queue to be processed
    while (pbft_mgr_->syncBlockQueueSize() > 10) {
      thisThreadSleepForMilliSeconds(10);
    }
    period++;

    if (period - 1 == conf_.test_params.rebuild_db_period) {
      break;
    }
  }
  while (pbft_mgr_->syncBlockQueueSize() > 0 || final_chain_->last_block_number() != period - 1) {
    thisThreadSleepForMilliSeconds(1000);
    LOG(log_nf_) << "Waiting on PBFT blocks to be processed. Queue size: " << pbft_mgr_->syncBlockQueueSize()
                 << " Chain size: " << final_chain_->last_block_number();
  }
}

uint64_t FullNode::getNumProposedBlocks() const { return BlockProposer::getNumProposedBlocks(); }

}  // namespace taraxa
