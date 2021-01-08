#include "executor.hpp"

#include "config.hpp"

namespace taraxa {

struct ReplayProtectionServiceDummy : ReplayProtectionService {
  bool is_nonce_stale(addr_t const &addr, uint64_t nonce) const override { return false; }
  void update(DbStorage::BatchPtr batch, round_t round, util::RangeView<TransactionInfo> const &trxs) override {}
};

Executor::Executor(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<FinalChain> final_chain,
                   std::shared_ptr<PbftChain> pbft_chain, uint32_t expected_max_trx_per_block)
    : replay_protection_service_(new ReplayProtectionServiceDummy),
      node_addr_(node_addr),
      db_(db),
      dag_mgr_(dag_mgr),
      trx_mgr_(trx_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain) {
  LOG_OBJECTS_CREATE("EXECUTOR");
  num_executed_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  transactions_tmp_buf_.reserve(expected_max_trx_per_block);
}

Executor::~Executor() { stop(); }

void Executor::setWSServer(std::shared_ptr<net::WSServer> ws_server) { ws_server_ = ws_server; }

void Executor::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Executor start...";
  exec_worker_ = std::make_unique<std::thread>([this]() { run(); });
}

void Executor::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cv_executor.notify_all();
  exec_worker_->join();
  LOG(log_nf_) << "Executor stopped";
}

void Executor::run() {
  LOG(log_nf_) << "Executor run...";
  uLock lock(shared_mutex_executor_);
  while (!stopped_) {
    cv_executor.wait(lock);
    executePbftBlocks_();
  }
}

void Executor::executePbftBlocks_() {
  while (pbft_chain_->hasUnexecutedBlocks()) {
    auto pbft_period = pbft_chain_->getPbftExecutedChainSize() + 1;
    auto pbft_block_hash = db_->getPeriodPbftBlock(pbft_period);
    if (!pbft_block_hash) {
      LOG(log_er_) << "DB corrupted - PBFT block period " << pbft_period
                   << " is not exist in DB period_pbft_block. PBFT chain size " << pbft_chain_->getPbftChainSize();
      assert(false);
    }
    // Get PBFT block in DB
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (!pbft_block) {
      LOG(log_er_) << "DB corrupted - Cannot find PBFT block hash " << pbft_block_hash
                   << " in PBFT chain DB pbft_blocks.";
      assert(false);
    }
    if (pbft_block->getPeriod() != pbft_period) {
      LOG(log_er_) << "DB corrupted - PBFT block hash " << pbft_block_hash << "has different period "
                   << pbft_block->getPeriod() << "in block data than in block order db: " << pbft_period;
      assert(false);
    }
    auto const &anchor_hash = pbft_block->getPivotDagBlockHash();
    auto finalized_dag_blk_hashes = std::move(*dag_mgr_->getDagBlockOrder(anchor_hash).second);

    auto batch = db_->createWriteBatch();
    {
      // This artificial scope will make sure we clean up the big chunk of memory allocated for this batch-processing
      // stuff as soon as possible
      transactions_tmp_buf_.clear();
      DbStorage::MultiGetQuery db_query(db_, transactions_tmp_buf_.capacity() + 100);
      auto dag_blks_raw = db_query.append(DbStorage::Columns::dag_blocks, finalized_dag_blk_hashes, false).execute();
      unordered_set<h256> unique_trxs;
      unique_trxs.reserve(transactions_tmp_buf_.capacity());
      for (auto const &dag_blk_raw : dag_blks_raw) {
        for (auto const &trx_h : DagBlock::extract_transactions_from_rlp(RLP(dag_blk_raw))) {
          if (!unique_trxs.insert(trx_h).second) {
            continue;
          }
          db_query.append(DbStorage::Columns::executed_transactions, trx_h);
          db_query.append(DbStorage::Columns::transactions, trx_h);
        }
      }
      auto trx_db_results = db_query.execute(false);
      for (uint i = 0; i < unique_trxs.size(); ++i) {
        auto has_been_executed = !trx_db_results[0 + i * 2].empty();
        if (has_been_executed) {
          continue;
        }
        // Non-executed trxs
        auto const &trx =
            transactions_tmp_buf_.emplace_back(vector_ref<string::value_type>(trx_db_results[1 + i * 2]).toBytes(),
                                               false, h256(db_query.get_key(1 + i * 2)));
        if (replay_protection_service_->is_nonce_stale(trx.getSender(), trx.getNonce())) {
          transactions_tmp_buf_.pop_back();
          continue;
        }
        static string const dummy_val = "_";
        db_->batch_put(*batch, DbStorage::Columns::executed_transactions, trx.getHash(), dummy_val);
      }
    }

    // Execute transactions in EVM(GO trx engine) and update Ethereum block
    auto const &[new_eth_header, trx_receipts, _] =
        final_chain_->advance(batch, pbft_block->getBeneficiary(), pbft_block->getTimestamp(), transactions_tmp_buf_);

    // Update replay protection service, like nonce watermark. Nonce watermark has been disabled
    replay_protection_service_->update(batch, pbft_period,
                                       util::make_range_view(transactions_tmp_buf_).map([](auto const &trx) {
                                         return ReplayProtectionService::TransactionInfo{
                                             trx.getSender(),
                                             trx.getNonce(),
                                         };
                                       }));

    // Update number of executed DAG blocks and transactions
    auto dag_blk_count = finalized_dag_blk_hashes.size();
    if (dag_blk_count != 0) {
      num_executed_blk_.fetch_add(dag_blk_count);
      num_executed_trx_.fetch_add(transactions_tmp_buf_.size());
      db_->addStatusFieldToBatch(StatusDbField::ExecutedBlkCount, num_executed_blk_, batch);
      db_->addStatusFieldToBatch(StatusDbField::ExecutedTrxCount, num_executed_trx_, batch);
      LOG(log_nf_) << node_addr_ << " :   Executed dag blocks index #" << num_executed_blk_ - dag_blk_count << "-"
                   << num_executed_blk_ - 1 << " , Transactions count: " << transactions_tmp_buf_.size();
    }

    // Add dag_block_period in DB
    for (auto const blk_hash : finalized_dag_blk_hashes) {
      db_->addDagBlockPeriodToBatch(blk_hash, pbft_period, batch);
    }

    // Update executed PBFT blocks size
    pbft_chain_->updateExecutedPbftChainSize();
    // Update PBFT chain head block
    db_->addPbftHeadToBatch(pbft_chain_->getHeadHash(), pbft_chain_->getJsonStr(), batch);

    // Set DAG blocks period
    dag_mgr_->setDagBlockOrder(anchor_hash, pbft_period, finalized_dag_blk_hashes, batch);

    // Remove executed transactions at Ethereum pending block. The Ethereum pending block is same with latest block at
    // Taraxa
    trx_mgr_->getPendingBlock()->advance(
        batch, new_eth_header.hash(),
        util::make_range_view(transactions_tmp_buf_).map([](auto const &trx) { return trx.getHash(); }));

    // Commit DB
    db_->commitWriteBatch(batch);
    LOG(log_nf_) << "DB write batch committed at period " << pbft_period << " PBFT block hash " << pbft_block_hash;

    // After DB commit, confirm in final chain(Ethereum)
    final_chain_->advance_confirm();

    // Creates snapshot if needed
    if (db_->createSnapshot(pbft_period)) {
      final_chain_->create_snapshot(pbft_period);
    }

    // Ethereum filter
    trx_mgr_->getFilterAPI()->note_block(new_eth_header.hash());
    trx_mgr_->getFilterAPI()->note_receipts(trx_receipts);

    // Update web server
    if (ws_server_) {
      ws_server_->newDagBlockFinalized(anchor_hash, pbft_period);
      ws_server_->newPbftBlockExecuted(*pbft_block, finalized_dag_blk_hashes);
      ws_server_->newEthBlock(new_eth_header);
    }
    LOG(log_nf_) << node_addr_ << " successful execute pbft block " << pbft_block_hash << " in period " << pbft_period;
  }
}

}  // namespace taraxa
