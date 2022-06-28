#include "node/node.hpp"

#include <libdevcore/CommonJS.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <stdexcept>

#include "dag/block_proposer.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block.hpp"
#include "network/rpc/Net.h"
#include "network/rpc/Taraxa.h"
#include "network/rpc/Test.h"
#include "network/rpc/eth/Eth.h"
#include "network/rpc/rpc_error_handler.hpp"
#include "pbft/pbft_manager.hpp"
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

  if (!conf_.chain.dag_genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  {
    if (conf_.db_config.rebuild_db) {
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, node_addr, true);
    }

    db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                      conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                      conf_.db_config.db_revert_to_period, node_addr, false,
                                      conf_.db_config.rebuild_db_columns);

    if (db_->hasMinorVersionChanged()) {
      LOG(log_si_) << "Minor DB version has changed. Rebuilding Db";
      conf_.db_config.rebuild_db = true;
      db_ = nullptr;
      old_db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                            conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                            conf_.db_config.db_revert_to_period, node_addr, true);
      db_ = std::make_shared<DbStorage>(conf_.db_path, conf_.db_config.db_snapshot_each_n_pbft_block,
                                        conf_.db_config.db_max_open_files, conf_.db_config.db_max_snapshots,
                                        conf_.db_config.db_revert_to_period, node_addr);
    }
    if (db_->getNumDagBlocks() == 0) {
      db_->saveDagBlock(conf_.chain.dag_genesis_block);
      db_->setGenesisHash(conf_.chain.genesisHash());
    }
  }
  LOG(log_nf_) << "DB initialized ...";

  gas_pricer_ = std::make_shared<GasPricer>(conf_.chain.gas_price.percentile, conf_.chain.gas_price.blocks,
                                            conf_.is_light_node, db_);
  final_chain_ = NewFinalChain(db_, conf_.chain, node_addr);
  trx_mgr_ = std::make_shared<TransactionManager>(conf_, db_, final_chain_, node_addr);

  auto genesis_hash = conf_.chain.genesisHash();
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
  auto dag_genesis_block_hash = conf_.chain.dag_genesis_block.getHash();

  pbft_chain_ = std::make_shared<PbftChain>(node_addr, db_);
  next_votes_mgr_ = std::make_shared<NextVotesManager>(node_addr, db_, final_chain_);
  dag_blk_mgr_ =
      std::make_shared<DagBlockManager>(node_addr, conf_.chain.sortition, conf_.chain.dag, db_, trx_mgr_, final_chain_,
                                        pbft_chain_, conf_.max_block_queue_warn, conf_.max_levels_per_period);
  dag_mgr_ = std::make_shared<DagManager>(dag_genesis_block_hash, node_addr, trx_mgr_, pbft_chain_, dag_blk_mgr_, db_,
                                          conf_.is_light_node, conf_.light_node_history, conf_.max_levels_per_period,
                                          conf_.dag_expiry_limit);
  vote_mgr_ = std::make_shared<VoteManager>(conf_.chain.pbft.committee_size, node_addr, db_, pbft_chain_, final_chain_,
                                            next_votes_mgr_);
  pbft_mgr_ = std::make_shared<PbftManager>(conf_.chain.pbft, dag_genesis_block_hash, node_addr, db_, pbft_chain_,
                                            vote_mgr_, next_votes_mgr_, dag_mgr_, dag_blk_mgr_, trx_mgr_, final_chain_,
                                            kp_.secret(), conf_.vrf_secret, conf_.max_levels_per_period);
  blk_proposer_ = std::make_shared<BlockProposer>(conf_.chain.dag.block_proposer, dag_mgr_, trx_mgr_, dag_blk_mgr_,
                                                  final_chain_, db_, node_addr, getSecretKey(), getVrfSecretKey());
  network_ = std::make_shared<Network>(conf_.network, genesis_hash, dev::p2p::Host::CapabilitiesFactory(),
                                       conf_.net_file_path().string(), kp_, db_, pbft_mgr_, pbft_chain_, vote_mgr_,
                                       next_votes_mgr_, dag_mgr_, dag_blk_mgr_, trx_mgr_);
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

  // Subscription to process hardforks
  // final_chain_->block_applying_.subscribe([&](uint64_t block_num) {
  //   // TODO: should have only common hardfork code calling hardfork executor
  //   auto &state_conf = conf_.chain.final_chain.state;
  //   if (state_conf.hardforks.fix_genesis_fork_block == block_num) {
  //     for (auto &e : state_conf.dpos->genesis_state) {
  //       for (auto &b : e.second) {
  //         b.second *= kOneTara;
  //       }
  //     }
  //     for (auto &b : state_conf.genesis_balances) {
  //       b.second *= kOneTara;
  //     }
  //     // we are multiplying it by TARA precision
  //     state_conf.dpos->eligibility_balance_threshold *= kOneTara;
  //     // amount of stake per vote should be 10 times smaller than eligibility threshold
  //     state_conf.dpos->vote_eligibility_balance_step.assign(state_conf.dpos->eligibility_balance_threshold);
  //     state_conf.dpos->eligibility_balance_threshold *= 10;
  //     conf_.overwrite_chain_config_in_file();
  //     final_chain_->update_state_config(state_conf);
  //   }
  // });

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
    blk_proposer_->setNetwork(network_);
    blk_proposer_->start();
  }

  pbft_mgr_->start();
  dag_mgr_->start();

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
  // Order of following stop calls MUST be like that, becasue dag_mgr_ depends on conditional variable in dag_blk_mgr_
  dag_blk_mgr_->stop();
  dag_mgr_->stop();
  LOG(log_nf_) << "Node stopped ... ";
}

void FullNode::rebuildDb() {
  pbft_mgr_->initialState();

  // Read pbft blocks one by one
  uint64_t period = 1;
  std::shared_ptr<PeriodData> period_data, next_period_data;
  std::vector<std::shared_ptr<Vote>> cert_votes;
  while (true) {
    if (next_period_data != nullptr)
      period_data = next_period_data;
    else {
      auto data = old_db_->getPeriodDataRaw(period);
      if (data.size() == 0) break;
      period_data = std::make_shared<PeriodData>(data);
    }
    auto data = old_db_->getPeriodDataRaw(period + 1);
    if (data.size() == 0) {
      next_period_data = nullptr;
      cert_votes = old_db_->getLastBlockCertVotes();
    } else {
      next_period_data = std::make_shared<PeriodData>(data);
      cert_votes = next_period_data->previous_block_cert_votes;
    }

    LOG(log_nf_) << "Adding PBFT block " << period_data->pbft_blk->getBlockHash().toString()
                 << " from old DB into syncing queue for processing";
    pbft_mgr_->periodDataQueuePush(std::move(*period_data), dev::p2p::NodeID(), std::move(cert_votes));
    pbft_mgr_->pushSyncedPbftBlocksIntoChain();

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

uint64_t FullNode::getNumProposedBlocks() const { return BlockProposer::getNumProposedBlocks(); }

}  // namespace taraxa
