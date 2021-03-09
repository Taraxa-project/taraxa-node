#include "full_node.hpp"

#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <stdexcept>

#include "consensus/block_proposer.hpp"
#include "consensus/pbft_manager.hpp"
#include "dag/dag.hpp"
#include "network/rpc/Net.h"
#include "network/rpc/Taraxa.h"
#include "network/rpc/Test.h"
#include "network/rpc/eth/Eth.h"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa {

using std::string;
using std::to_string;

FullNode::FullNode(FullNodeConfig const &conf)
    : conf_(conf),
      kp_(conf_.node_secret.empty()
              ? dev::KeyPair::create()
              : dev::KeyPair(dev::Secret(conf_.node_secret, dev::Secret::ConstructFromStringType::FromHex))) {}

void FullNode::init() {
  fs::create_directories(conf_.db_path);
  // Initialize logging
  auto const &node_addr = kp_.address();

  for (auto &logging : conf_.log_configs) {
    logging.InitLogging(node_addr);
  }

  LOG_OBJECTS_CREATE("FULLND");
  log_time_ = logger::createLogger(logger::Verbosity::Info, "TMSTM", node_addr);

  LOG(log_si_) << "Node public key: " << EthGreen << kp_.pub().toString() << std::endl
               << "Node address: " << EthRed << node_addr.toString() << std::endl
               << "Node VRF public key: " << EthGreen << vrf_wrapper::getVrfPublicKey(conf_.vrf_secret).toString();

  if (!conf_.chain.dag_genesis_block.verifySig()) {
    LOG(log_er_) << "Genesis block is invalid";
    assert(false);
  }
  if (conf_.test_params.rebuild_db) {
    old_db_ = DB::make(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                       conf_.test_params.db_max_snapshots, conf_.test_params.db_revert_to_period, node_addr, true);
  }

  db_ = DB::make(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block, conf_.test_params.db_max_snapshots,
                 conf_.test_params.db_revert_to_period, node_addr);

  if (db_->hasMinorVersionChanged()) {
    LOG(log_si_) << "Minor DB version has changed. Rebuilding Db";
    conf_.test_params.rebuild_db = true;
    db_ = nullptr;
    old_db_ = DB::make(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block,
                       conf_.test_params.db_max_snapshots, conf_.test_params.db_revert_to_period, node_addr, true);
    db_ = DB::make(conf_.db_path, conf_.test_params.db_snapshot_each_n_pbft_block, conf_.test_params.db_max_snapshots,
                   conf_.test_params.db_revert_to_period, node_addr);
  }
  register_s_ptr(db_);
  LOG(log_nf_) << "DB initialized ...";

  final_chain_ = NewFinalChain(db_, conf_.chain.final_chain, conf_.opts_final_chain, node_addr);
  register_s_ptr(final_chain_);
  emplace(trx_mgr_, conf_, node_addr, db_, log_time_);
  auto genesis_hash = conf_.chain.dag_genesis_block.getHash().toString();
  emplace(pbft_chain_, genesis_hash, node_addr, db_);
  emplace(dag_mgr_, conf_.chain.dag_genesis_block, node_addr, trx_mgr_, pbft_chain_, db_);
  emplace(dag_blk_mgr_, node_addr, conf_.chain.vdf, conf_.chain.final_chain.state.dpos, 1024 /*capacity*/,
          4 /* verifer thread*/, db_, trx_mgr_, final_chain_, pbft_chain_, log_time_,
          conf_.test_params.max_block_queue_warn);
  emplace(vote_mgr_, node_addr, final_chain_, pbft_chain_);
  emplace(trx_order_mgr_, node_addr, db_);
  emplace(pbft_mgr_, conf_.chain.pbft, genesis_hash, node_addr, db_, pbft_chain_, vote_mgr_, dag_mgr_, dag_blk_mgr_,
          final_chain_, kp_.secret(), conf_.vrf_secret);
  emplace(blk_proposer_, conf_.test_params.block_proposer, conf_.chain.vdf, dag_mgr_, trx_mgr_, dag_blk_mgr_,
          final_chain_, node_addr, getSecretKey(), getVrfSecretKey(), log_time_);
  emplace(network_, conf_.network, conf_.net_file_path().string(), kp_.secret(), genesis_hash, node_addr, db_,
          pbft_mgr_, pbft_chain_, vote_mgr_, dag_mgr_, dag_blk_mgr_, trx_mgr_, kp_.pub(),
          conf_.chain.pbft.lambda_ms_min);

  LOG(log_time_) << "Start taraxa efficiency evaluation logging:" << std::endl;
}

void FullNode::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  if (conf_.rpc) {
    emplace(rpc_thread_pool_, conf_.rpc->threads_num);
    net::rpc::eth::EthParams eth_rpc_params;
    eth_rpc_params.address = getAddress();
    eth_rpc_params.secret = kp_.secret();
    eth_rpc_params.chain_id = conf_.chain.chain_id;
    eth_rpc_params.final_chain = final_chain_;
    eth_rpc_params.get_trx = [db = db_](auto const &trx_hash) { return db->getTransaction(trx_hash); };
    eth_rpc_params.send_trx = [trx_manager = trx_mgr_](auto const &trx) {
      auto [ok, err_msg] = trx_manager->insertTransaction(trx, true);
      if (!ok) {
        BOOST_THROW_EXCEPTION(
            runtime_error(util::fmt("Transaction is rejected.\n"
                                    "RLP: %s\n"
                                    "Reason: %s",
                                    dev::toJS(*trx.rlp()), err_msg)));
      }
    };
    auto eth_json_rpc = net::rpc::eth::NewEth(move(eth_rpc_params));
    emplace(jsonrpc_api_,
            new net::Test(getShared()),    //
            new net::Taraxa(getShared()),  //
            new net::Net(getShared()),     //
            eth_json_rpc);
    if (conf_.rpc->http_port) {
      emplace(jsonrpc_http_, rpc_thread_pool_->unsafe_get_io_context(),
              boost::asio::ip::tcp::endpoint{conf_.rpc->address, *conf_.rpc->http_port}, getAddress());
      jsonrpc_api_->addConnector(jsonrpc_http_);
      jsonrpc_http_->StartListening();
    }
    if (conf_.rpc->ws_port) {
      emplace(jsonrpc_ws_, rpc_thread_pool_->unsafe_get_io_context(),
              boost::asio::ip::tcp::endpoint{conf_.rpc->address, *conf_.rpc->ws_port}, getAddress());
      jsonrpc_api_->addConnector(jsonrpc_ws_);
      jsonrpc_ws_->run();
    }
    auto const &ws = jsonrpc_ws_;
    final_chain_->block_executed.subscribe(
        [=](auto const &obj) {
          eth_json_rpc->note_block_executed(*obj.final_chain_blk, obj.trxs, obj.trx_receipts);
          if (ws) {
            ws->newDagBlockFinalized(obj.pbft_blk->getPivotDagBlockHash(), obj.pbft_blk->getPeriod());
            ws->newPbftBlockExecuted(*obj.pbft_blk, obj.finalized_dag_blk_hashes);
            ws->newEthBlock(*obj.final_chain_blk);
          }
        },
        util::ThreadPool::as_task_executor(rpc_thread_pool_));
    trx_mgr_->transaction_accepted.subscribe(
        [=](auto const &trx_hash) {
          eth_json_rpc->note_pending_transactions(vector{trx_hash});
          if (ws) {
            ws->newPendingTransaction(trx_hash);
          }
        },
        util::ThreadPool::as_task_executor(rpc_thread_pool_));
  }

  if (conf_.network.network_is_boot_node) {
    LOG(log_nf_) << "Starting a boot node ..." << std::endl;
  }
  if (!conf_.test_params.rebuild_db) {
    network_->start(conf_.network.network_is_boot_node);
  }
  trx_mgr_->setNetwork(network_);
  trx_mgr_->start();
  if (!conf_.test_params.rebuild_db) {
    blk_proposer_->setNetwork(network_);
    blk_proposer_->start();
  }
  pbft_mgr_->setNetwork(network_);
  pbft_mgr_->start();
  dag_blk_mgr_->start();
  block_workers_.emplace_back([this]() {
    while (!stopped_) {
      // will block if no verified block available
      auto blk = dag_blk_mgr_->popVerifiedBlock();

      if (!stopped_) {
        received_blocks_++;
      }

      if (dag_mgr_->pivotAndTipsAvailable(blk)) {
        dag_mgr_->addDagBlock(blk);
        if (jsonrpc_ws_) {
          jsonrpc_ws_->newDagBlock(blk);
        }
        network_->onNewBlockVerified(blk);
        LOG(log_time_) << "Broadcast block " << blk.getHash() << " at: " << getCurrentTimeMilliSeconds();
      } else {
        // Networking makes sure that dag block that reaches queue already had
        // its pivot and tips processed This should happen in a very rare case
        // where in some race condition older block is verfified faster then
        // new block but should resolve quickly, return block to queue
        if (!stopped_) {
          if (dag_blk_mgr_->pivotAndTipsValid(blk)) {
            LOG(log_dg_) << "Block could not be added to DAG " << blk.getHash().toString();
            received_blocks_--;
            dag_blk_mgr_->pushVerifiedBlock(blk);
          }
        }
      }
    }
  });

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
  blk_proposer_->stop();
  blk_proposer_->setNetwork(nullptr);
  pbft_mgr_->stop();
  pbft_mgr_->setNetwork(nullptr);
  trx_mgr_->stop();
  trx_mgr_->setNetwork(nullptr);
  network_->stop();
  dag_blk_mgr_->stop();
  for (auto &t : block_workers_) {
    t.join();
  }
  LOG(log_nf_) << "Node stopped ... ";
}

void FullNode::rebuildDb() {
  // Read pbft blocks one by one
  uint64_t period = 1;

  while (true) {
    std::map<uint64_t, std::map<blk_hash_t, std::pair<DagBlock, std::vector<Transaction>>>> dag_blocks_per_level;
    auto pbft_hash = old_db_->getPeriodPbftBlock(period);
    if (pbft_hash == nullptr) {
      break;
    }
    auto pbft_block = old_db_->getPbftBlock(*pbft_hash);
    auto pivot_dag_hash = pbft_block->getPivotDagBlockHash();
    std::set<blk_hash_t> pbft_dag_blocks;
    std::vector<blk_hash_t> dag_blocks;
    pbft_dag_blocks.emplace(pivot_dag_hash);
    dag_blocks.push_back(pivot_dag_hash);

    // Read all the dag blocks from the pbft period
    while (!dag_blocks.empty()) {
      std::vector<blk_hash_t> new_dag_blocks;
      for (auto &dag_block_hash : dag_blocks) {
        auto dag_block = old_db_->getDagBlock(dag_block_hash);
        auto pivot_hash = dag_block->getPivot();
        auto tips = dag_block->getTips();
        if (pbft_dag_blocks.count(pivot_hash) == 0 && !(dag_blk_mgr_->isBlockKnown(pivot_hash))) {
          pbft_dag_blocks.emplace(pivot_hash);
          new_dag_blocks.push_back(pivot_hash);
        }
        for (auto &tip : tips) {
          if (pbft_dag_blocks.count(tip) == 0 && !(dag_blk_mgr_->isBlockKnown(tip))) {
            pbft_dag_blocks.emplace(tip);
            new_dag_blocks.push_back(tip);
          }
        }
      }
      dag_blocks = new_dag_blocks;
    }

    // Read the transactions for dag blocks
    for (auto &dag_block_hash : pbft_dag_blocks) {
      std::vector<Transaction> transactions;
      auto dag_block = old_db_->getDagBlock(dag_block_hash);

      DB::MultiGetQuery db_query(old_db_);
      db_query.append(DB::Columns::transactions, dag_block->getTrxs());
      auto db_response = db_query.execute();
      for (auto &db_trx : db_response) {
        transactions.push_back(Transaction(asBytes(db_trx)));
      }
      dag_blocks_per_level[dag_block->getLevel()][dag_block_hash] = std::make_pair(*dag_block, transactions);
    }

    // Add pbft blocks with votes in queue
    auto db_votes = old_db_->getVotes(*pbft_hash);
    vector<Vote> votes;
    for (auto const &el : RLP(db_votes)) {
      votes.emplace_back(el);
    }
    PbftBlockCert pbft_blk_and_votes(*pbft_block, votes);
    LOG(log_nf_) << "Adding pbft block into queue " << pbft_block->getBlockHash().toString();
    pbft_chain_->setSyncedPbftBlockIntoQueue(pbft_blk_and_votes);

    // Add dag blocks and transactions from above to the queue
    for (auto const &block_level : dag_blocks_per_level) {
      for (auto const &block : block_level.second) {
        LOG(log_nf_) << "Storing block " << block.second.first.getHash().toString() << " with "
                     << block.second.second.size() << " transactions";
        dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block.second.first, block.second.second);
      }
    }

    // Wait if more than 10 pbft blocks in queue to be processed
    while (pbft_chain_->pbftSyncedQueueSize() > 10) {
      thisThreadSleepForMilliSeconds(10);
    }
    period++;

    if (period - 1 == conf_.test_params.rebuild_db_period) {
      break;
    }
  }
  while (pbft_chain_->pbftSyncedQueueSize() > 0 || final_chain_->last_block_number() != period - 1) {
    thisThreadSleepForMilliSeconds(1000);
    LOG(log_nf_) << "Waiting on PBFT blocks to be processed. Queue size: " << pbft_chain_->pbftSyncedQueueSize()
                 << " Chain size: " << final_chain_->last_block_number();
  }
}

uint64_t FullNode::getNumProposedBlocks() const { return BlockProposer::getNumProposedBlocks(); }

FullNode::Handle::Handle(FullNodeConfig const &conf, bool start) : shared_ptr_t(new FullNode(conf)) {
  get()->init();
  if (start) {
    get()->start();
  }
}

FullNode::Handle::~Handle() {
  auto node = get();
  if (!node) {
    return;
  }
  node->close();
  shared_ptr_t::weak_type node_weak = *this;
  reset();
  assert(node_weak.use_count() == 0);
}

}  // namespace taraxa
