/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:46
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:11:46
 */
#include "executor.hpp"
#include "full_node.hpp"
#include "util.hpp"
#include "vm/TaraxaVM.hpp"

using namespace std;
namespace taraxa {

string LAST_STATE_ROOT_KEY = "last_root";

Executor::~Executor() {
  if (!stopped_) {
    stop();
  }
}
void Executor::start() {
  if (!stopped_) return;
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
}
bool Executor::executeBlkTrxs(blk_hash_t const& blk) {
  string blk_json = db_blks_->get(blk.toString());
  if (blk_json.empty()) {
    LOG(log_er_) << "Cannot get block from db: " << blk << std::endl;
    return false;
  }
  DagBlock dag_block(blk_json);
  vm::Block vmBlock{
      dag_block.sender(), 1, 0, 0, 100000000000, dag_block.getHash(),
  };
  auto& log_time = node_.lock()->getTimeLogger();
  auto trxs_hash = dag_block.getTrxs();
  for (auto const& trx_hash : trxs_hash) {
    LOG(log_time) << "Transaction " << trx_hash
                  << " read from db at: " << getCurrentTimeMilliSeconds();
    Transaction trx(db_trxs_->get(trx_hash.toString()));
    if (!trx.getHash()) {
      LOG(log_er_) << "Transaction is invalid" << std::endl;
      continue;
    }
    const auto& receiver = trx.getReceiver();
    vmBlock.transactions.push_back({
        receiver ? make_optional(receiver) : nullopt,
        trx.getSender(),
        trx.getNonce(),
        trx.getValue(),
        trx.getGas(),
        trx.getGasPrice(),
        trx.getData(),
        trx.getHash(),
    });
  }
  const auto& lastRootStr = db_accs_->get(LAST_STATE_ROOT_KEY);
  const auto& lastRoot = lastRootStr.size() == 0 ? h256() : h256(lastRootStr);
  const auto& result = taraxaVM->transitionState({lastRoot, vmBlock});
  db_accs_->update(LAST_STATE_ROOT_KEY, toHexPrefixed(result.stateRoot));
//  if (node_.lock()) { TODO logging
//    LOG(log_time) << "Block " << blk
//                  << " executed at: " << getCurrentTimeMilliSeconds();
//  }
  return true;
}
bool Executor::execute(TrxSchedule const& sche) {
  for (auto const& blk : sche.blk_order) {
    if (!executeBlkTrxs(blk)) {
      return false;
    }
  }
  return true;
}

}