/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */

#include "executor.hpp"
#include "full_node.hpp"
#include "transaction.hpp"

namespace taraxa {

bool Executor::execute(TrxSchedule const& schedule,
                       BalanceTable& sortition_account_balance_table,
                       uint64_t period) {
  if (schedule.blk_order.empty()) {
    return true;
  }
  for (auto blk_i(0); blk_i < schedule.blk_order.size(); ++blk_i) {
    auto blk_hash = schedule.blk_order[blk_i];
    auto blk_bytes = db_blks_->get(blk_hash);
    if (blk_bytes.size() == 0) {
      LOG(log_er_) << "Cannot get block from db: " << blk_hash << std::endl;
      return false;
    }
    if (executed_blk_.get(blk_hash).second == true) {
      LOG(log_er_) << "Block " << blk_hash << " has been executed ...";
      return false;
    }
    DagBlock dag_block(blk_bytes);
    auto trx_modes = schedule.vec_trx_modes[blk_i];
    auto trxs_hashes = dag_block.getTrxs();
    auto num_trxs = trxs_hashes.size();
    int num_overlapped_trx(0);
    auto current_snapshot = state_registry_->getCurrentSnapshot();
    trx_engine::StateTransitionRequest st_transition_req{
        current_snapshot.state_root,
        trx_engine::Block{
            current_snapshot.block_number + 1,
            dag_block.getSender(),
            dag_block.getTimestamp(),
            0,
            // TODO unint64 is correct
            uint64_t(MOCK_BLOCK_GAS_LIMIT),
            blk_hash,
        },
    };
    for (auto trx_i(0); trx_i < num_trxs; ++trx_i) {
      auto const& trx_hash = trxs_hashes[trx_i];
      auto mode = trx_modes[trx_i];
      if (mode == 0) {
        LOG(log_dg_) << "Transaction " << trx_hash << "in block " << blk_hash
                     << " is overlapped";
        num_overlapped_trx++;
        continue;
      }
      LOG(log_time_) << "Transaction " << trx_hash
                     << " read from db at: " << getCurrentTimeMilliSeconds();
      Transaction trx(db_trxs_->get(trx_hash));
      if (!trx.getHash()) {
        LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
        continue;
      }
      auto const& receiver = trx.getReceiver();
      st_transition_req.block.transactions.push_back(trx_engine::Transaction{
          receiver.isZero() ? std::nullopt : std::optional(receiver),
          trx.getSender(),
          // TODO unint64 is correct
          uint64_t(trx.getNonce()),
          trx.getValue(),
          // TODO unint64 is correct
          uint64_t(trx.getGas()),
          trx.getGasPrice(),
          trx.getData(),
          trx_hash,
      });
    }
    if (!st_transition_req.block.transactions.empty()) {
      auto result = trx_engine_.transitionState(st_transition_req);
      trx_engine_.commitToDisk(result.stateRoot);
      state_registry_->append({{blk_hash, result.stateRoot}});
      for (auto const& [addr, balance] : result.updatedBalances) {
        // TODO update the sortition table
        std::cout << "UPDATED BALANCE: " << addr << " : " << balance
                  << std::endl;
      }
      for (auto const& trx : st_transition_req.block.transactions) {
        num_executed_trx_.fetch_add(1);
        executed_trx_.insert(trx.hash, true);
        LOG(log_time_) << "Transaction " << trx.hash
                       << " executed at: " << getCurrentTimeMilliSeconds();
      }
    }
    num_executed_blk_.fetch_add(1);
    executed_blk_.insert(blk_hash, true);
    LOG(log_nf_) << full_node_.lock()->getAddress() << ": Block number "
                 << num_executed_blk_ << ": " << blk_hash
                 << " executed, Efficiency: " << (num_trxs - num_overlapped_trx)
                 << " / " << num_trxs;
  }
  return true;
}

}  // namespace taraxa