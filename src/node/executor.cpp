#include "executor.hpp"

#include "config/config.hpp"

namespace taraxa {

struct ReplayProtectionServiceDummy : ReplayProtectionService {
  bool is_nonce_stale(addr_t const & /*addr*/, uint64_t /*nonce*/) const override { return false; }
  void update(DbStorage::BatchPtr /*batch*/, round_t /*round*/,
              util::RangeView<TransactionInfo> const & /*trxs*/) override {}
};

Executor::Executor(addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<DagManager> dag_mgr,
                   std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                   std::shared_ptr<FinalChain> final_chain, std::shared_ptr<PbftChain> pbft_chain,
                   uint32_t expected_max_trx_per_block)
    : replay_protection_service_(new ReplayProtectionServiceDummy),
      db_(db),
      dag_mgr_(dag_mgr),
      trx_mgr_(trx_mgr),
      dag_blk_mgr_(dag_blk_mgr),
      final_chain_(final_chain),
      pbft_chain_(pbft_chain),
      node_addr_(node_addr) {
  LOG_OBJECTS_CREATE("EXECUTOR");
  if (auto last_period = pbft_chain_->getPbftChainSize(); final_chain_->last_block_number() < last_period) {
    to_execute_ = load_pbft_blk(last_period);
  }
  num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  transactions_tmp_buf_.reserve(expected_max_trx_per_block);
}

Executor::~Executor() { stop(); }

void Executor::setWSServer(std::shared_ptr<net::WSServer> ws_server) { ws_server_ = move(ws_server); }

void Executor::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  LOG(log_nf_) << "Executor start...";
  exec_worker_ = std::make_unique<std::thread>([this]() {
    LOG(log_nf_) << "Executor run...";
    while (!stopped_) {
      tick();
    }
  });
}

void Executor::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  cv_.notify_one();
  exec_worker_->join();
  LOG(log_nf_) << "Executor stopped";
}

void Executor::execute(std::shared_ptr<PbftBlock> blk) {
  assert(final_chain_->last_block_number() < blk->getPeriod());
  {
    std::unique_lock l(mu_);
    if (to_execute_) {
      assert(to_execute_->getPeriod() < blk->getPeriod());
    }
    to_execute_ = move(blk);
  }
  cv_.notify_one();
}

void Executor::executeSynced(const std::shared_ptr<PbftBlock> &blk) {
  assert(final_chain_->last_block_number() < blk->getPeriod());

  execute_(*blk);
}

void Executor::tick() {
  std::shared_ptr<PbftBlock> pbft_block;
  {
    std::unique_lock l(mu_);
    if (!to_execute_) {
      cv_.wait(l);
    }
    if (stopped_) {
      return;
    }
    std::swap(pbft_block, to_execute_);
  }
  for (auto blk_n = final_chain_->last_block_number() + 1; blk_n < pbft_block->getPeriod(); ++blk_n) {
    execute_(*load_pbft_blk(blk_n));
  }
  execute_(*pbft_block);
}

void Executor::execute_(PbftBlock const &pbft_block) {
  auto pbft_period = pbft_block.getPeriod();
  auto const &pbft_block_hash = pbft_block.getBlockHash();
  auto const &anchor_hash = pbft_block.getPivotDagBlockHash();
  auto finalized_dag_blk_hashes = db_->getFinalizedDagBlockHashesByAnchor(anchor_hash);
  auto batch = db_->createWriteBatch();
  {
    // This artificial scope will make sure we clean up the big chunk of memory allocated for this batch-processing
    // stuff as soon as possible
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
      auto const &trx = transactions_tmp_buf_.emplace_back(&trx_db_results[1 + i * 2], dev::eth::CheckTransaction::None,
                                                           true, h256(db_query.get_key(1 + i * 2)));
      if (replay_protection_service_->is_nonce_stale(trx.sender(), trx.nonce())) {
        transactions_tmp_buf_.pop_back();
        continue;
      }
      static string const dummy_val = "_";
      db_->batch_put(*batch, DbStorage::Columns::executed_transactions, trx.sha3(), dummy_val);
    }
  }

  // Execute transactions in EVM(GO trx engine) and update Ethereum block
  auto const &[new_eth_header, trx_receipts, _] =
      final_chain_->advance(batch, pbft_block.getBeneficiary(), pbft_block.getTimestamp(), transactions_tmp_buf_);

  // Update replay protection service, like nonce watermark. Nonce watermark has been disabled
  replay_protection_service_->update(batch, pbft_period,
                                     util::make_range_view(transactions_tmp_buf_).map([](auto const &trx) {
                                       return ReplayProtectionService::TransactionInfo{
                                           trx.from(),
                                           trx.nonce(),
                                       };
                                     }));

  // Update number of executed DAG blocks and transactions
  auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
  auto num_executed_trx = num_executed_trx_ + transactions_tmp_buf_.size();
  if (!finalized_dag_blk_hashes.empty()) {
    db_->addStatusFieldToBatch(StatusDbField::ExecutedBlkCount, num_executed_dag_blk, batch);
    db_->addStatusFieldToBatch(StatusDbField::ExecutedTrxCount, num_executed_trx, batch);
    LOG(log_nf_) << node_addr_ << " :   Executed dag blocks index #"
                 << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-" << num_executed_dag_blk_ - 1
                 << " , Transactions count: " << transactions_tmp_buf_.size();
  }

  // Update proposal period DAG levels map
  auto anchor = db_->getDagBlock(anchor_hash);
  if (!anchor) {
    LOG(log_er_) << "DB corrupted - Cannot find anchor block: " << anchor_hash << " in DB.";
    assert(false);
  }
  auto new_proposal_period_levels_map = dag_blk_mgr_->newProposePeriodDagLevelsMap(anchor->getLevel());
  db_->addProposalPeriodDagLevelsMapToBatch(*new_proposal_period_levels_map, batch);
  auto dpos_current_max_proposal_period = dag_blk_mgr_->getCurrentMaxProposalPeriod();
  db_->addDposProposalPeriodLevelsFieldToBatch(DposProposalPeriodLevelsStatus::max_proposal_period,
                                               dpos_current_max_proposal_period, batch);

  // Commit DB
  {
    rocksdb::WriteOptions opts;
    opts.sync = true;
    db_->commitWriteBatch(batch, opts);
  }
  LOG(log_nf_) << "DB write batch committed at period " << pbft_period << " PBFT block hash " << pbft_block_hash;

  // After DB commit, confirm in final chain(Ethereum)
  final_chain_->advance_confirm();

  // Only NOW we are fine to modify in-memory states as they have been backed by the db

  num_executed_dag_blk_ = num_executed_dag_blk;
  num_executed_trx_ = num_executed_trx;

  // Creates snapshot if needed
  if (db_->createSnapshot(pbft_period)) {
    final_chain_->create_snapshot(pbft_period);
  }

  // Update web server
  if (ws_server_) {
    ws_server_->newDagBlockFinalized(anchor_hash, pbft_period);
    ws_server_->newPbftBlockExecuted(pbft_block, finalized_dag_blk_hashes);
    ws_server_->newEthBlock(new_eth_header);
  }

  transactions_tmp_buf_.clear();

  LOG(log_nf_) << node_addr_ << " successful execute pbft block " << pbft_block_hash << " in period " << pbft_period;
}

std::shared_ptr<PbftBlock> Executor::load_pbft_blk(uint64_t pbft_period) {
  auto pbft_block_hash = db_->getPeriodPbftBlock(pbft_period);
  if (!pbft_block_hash) {
    LOG(log_er_) << "DB corrupted - PBFT block period " << pbft_period
                 << " does not exist in DB period_pbft_block. PBFT chain size " << pbft_chain_->getPbftChainSize();
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
                 << pbft_block->getPeriod() << " in block data than in block order db: " << pbft_period;
    assert(false);
  }
  return pbft_block;
}

}  // namespace taraxa
