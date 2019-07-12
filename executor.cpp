/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "executor.hpp"
#include "full_node.hpp"
#include "pbft_manager.hpp"
#include "util.hpp"
#include "vm/TaraxaVM.hpp"

using namespace std;

namespace taraxa {

// TODO
// string LAST_STATE_ROOT_KEY = "last_root";

Executor::~Executor() {
  if (!stopped_) {
    stop();
  }
}
void Executor::start() {
  if (!stopped_) {
    return;
  }
  if (!node_.lock()) {
    LOG(log_er_) << "FullNode is not set ..." << std::endl;
    return;
  }
  db_blks_ = node_.lock()->getBlksDB();
  db_trxs_ = node_.lock()->getTrxsDB();
  db_accs_ = node_.lock()->getAccsDB();
  taraxaVM = node_.lock()->getVM();
  stopped_ = false;
}
void Executor::stop() {
  if (stopped_) return;
  db_blks_ = nullptr;
  db_trxs_ = nullptr;
  db_accs_ = nullptr;
  taraxaVM = nullptr;
  stopped_ = true;
}
bool Executor::executeBlkTrxs(
    blk_hash_t const& blk,
    unordered_map<addr_t, bal_t>& sortition_account_balance_table) {
  std::unique_lock l(mu);
  string blk_json = db_blks_->get(blk.toString());
  if (blk_json.empty()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  DagBlock dag_block(blk_json);
  vm::Block vmBlock{
      dag_block.sender(), 1, 0, 0, MOCK_BLOCK_GAS_LIMIT, dag_block.getHash(),
  };
  auto& log_time = node_.lock()->getTimeLogger();
  auto trxs_hash = dag_block.getTrxs();
  for (auto const& trx_hash : trxs_hash) {
    LOG(log_time) << "Transaction " << trx_hash
                  << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid: " << trx << std::endl;
      continue;
    }
    // TODO remove
    if (!txHashes.insert(trx.getHash()).second) {
      cerr << endl << "Duplicate tx" << endl;
      exit(1);
    }
    const auto& receiver = trx.getReceiver();
    vmBlock.transactions.push_back({
        receiver ? make_optional(receiver) : nullopt,
        trx.getSender(),
        trx.getNonce(),
        trx.getValue(),
        uint64_t(val_t::Arith(trx.getGas())),
        trx.getGasPrice(),
        trx.getData(),
        trx.getHash(),
    });
    //        coinTransfer(trx, sortition_account_balance_table);
  }
  //    db_trxs_->commit();

  const auto& baseRoot = db_accs_->getState<boost::shared_lock>()->rootHash();
  const auto& result = taraxaVM->transitionState({baseRoot, vmBlock});
  // TODO: use persistent storage to track the reference
  // to the last committed block, and add stateRoot field to blocks
  db_accs_->getState<boost::unique_lock>()->setRoot(result.stateRoot);
  //  if (node_.lock()) {
  // TODO: logging
  //  LOG(log_time) << "Block " << blk << " executed at: " <<
  //  getCurrentTimeMilliSeconds();
  //  }
  return true;
}
bool Executor::execute(
    TrxSchedule const& sche,
    unordered_map<addr_t, bal_t>& sortition_account_balance_table) {
  for (auto const& blk : sche.blk_order) {
    if (!executeBlkTrxs(blk, sortition_account_balance_table)) {
      return false;
    }
  }
  return true;
}

bool Executor::coinTransfer(
    Transaction const& trx,
    std::unordered_map<addr_t, bal_t>& sortition_account_balance_table) {
  addr_t sender = trx.getSender();
  addr_t receiver = trx.getReceiver();
  bal_t value = trx.getValue();
  auto sender_bal = db_accs_->get(sender.toString());
  auto receiver_bal = db_accs_->get(receiver.toString());
  bal_t sender_initial_coin = sender_bal.empty() ? 0 : stoull(sender_bal);
  bal_t receiver_initial_coin = receiver_bal.empty() ? 0 : stoull(receiver_bal);

  if (sender_initial_coin < trx.getValue()) {
    LOG(log_er_) << "Insufficient fund for transfer ... , sender " << sender
                 << " , sender balance: " << sender_initial_coin
                 << " , transfer: " << value << std::endl;
    return false;
  }
  if (receiver_initial_coin >
      std::numeric_limits<uint64_t>::max() - trx.getValue()) {
    LOG(log_er_) << "Error! Fund can overflow ..." << std::endl;
    return false;
  }
  bal_t new_sender_bal = sender_initial_coin - value;
  bal_t new_receiver_bal = receiver_initial_coin + value;
  db_accs_->update(sender.toString(), std::to_string(new_sender_bal));
  db_accs_->update(receiver.toString(), std::to_string(new_receiver_bal));
  // Update account balance table. Will remove in VM since vm return a list of
  // modified balance accounts
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_er_) << "Full node unavailable" << std::endl;
    return false;
  }
  size_t pbft_require_sortition_coins =
      full_node->getPbftManager()->VALID_SORTITION_COINS;
  if (new_sender_bal >= pbft_require_sortition_coins) {
    sortition_account_balance_table[sender] = new_sender_bal;
  } else if (sortition_account_balance_table.find(sender) !=
             sortition_account_balance_table.end()) {
    sortition_account_balance_table.erase(sender);
  }
  if (new_receiver_bal >= pbft_require_sortition_coins) {
    sortition_account_balance_table[receiver] = new_receiver_bal;
  }

  LOG(log_nf_) << "New sender bal: " << new_sender_bal << std::endl;
  LOG(log_nf_) << "New receiver bal: " << new_receiver_bal << std::endl;

  return true;
}

}  // namespace taraxa