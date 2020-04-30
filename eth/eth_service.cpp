#include "eth_service.hpp"

#include <libdevcore/TrieHash.h>
#include <libethashseal/Ethash.h>

#include <stdexcept>

#include "./util.hpp"
#include "full_node.hpp"
#include "taraxa_seal_engine.hpp"
#include "util/once_out_of_scope.hpp"

namespace taraxa::eth::eth_service {
using dev::bytesConstRef;
using dev::BytesMap;
using dev::hash256;
using dev::Invalid256;
using dev::KeyPair;
using dev::rlp;
using dev::rlpList;
using dev::RLPStream;
using dev::eth::Ethash;
using dev::eth::FixedAccountHolder;
using dev::eth::IncludeSeal;
using dev::eth::LogBloom;
using dev::eth::Permanence;
using dev::eth::VerifiedBlockRef;
using std::move;
using std::unique_lock;
using taraxa_seal_engine::TaraxaSealEngine;

using err = std::runtime_error;

bytes const bytes_zero;
h256 const h256_zero(0);
u256 const u256_zero(0);
Nonce const block_nonce_zero(0);

EthService::EthService(shared_ptr<FullNode> const& node,  //
                       BlockChain::CacheConfig const& cache_config,
                       milliseconds gc_trigger_backoff)
    : node_(node),
      db_adapter_(
          new DatabaseAdapter(node->getDB(), DbStorage::Columns::aleth_chain)),
      extras_db_adapter_(new DatabaseAdapter(
          node->getDB(), DbStorage::Columns::aleth_chain_extras)),
      bc_(db_adapter_, extras_db_adapter_, cache_config),
      bc_gc_thread_([this, gc_trigger_backoff] {
        while (!destructor_called_) {
          std::this_thread::sleep_for(gc_trigger_backoff);
          bc_.garbageCollect();
        }
      }) {}

EthService::~EthService() {
  destructor_called_ = true;
  gc_thread_.join();
}


void EthService::populateTransactionWithDefaults(
    TransactionSkeleton& ret) const {
  if (!ret.from) {
    ret.from = author();
  }
  if (ret.nonce == Invalid256) {
    ret.nonce = nonceAt(ret.from);
  }
  if (ret.gasPrice == Invalid256) {
    ret.gasPrice = gasBidPrice();
  }
  if (ret.gas == Invalid256) {
    static const u256 defaultTransactionGas = 90000;
    ret.gas = defaultTransactionGas;
  }
}

Transaction EthService::sign(TransactionSkeleton const& _t) {
  return {_t, node_.lock()->getSecretKey()};
}

h256 EthService::importTransaction(Transaction const& _t) {
  auto taraxa_trx = util::trx_eth_2_taraxa(_t);
  auto ok = node_.lock()->insertTransaction(taraxa_trx, true);
  if (!ok.first) {
    throw(err("could not insert transaction - " + ok.second));
  }
  return taraxa_trx.getHash();
}

BlockHeader EthService::commitBlock(DbStorage::BatchPtr batch,
                                    Address const& author, int64_t timestamp,
                                    Transactions const& transactions) {
  auto current_header = head();
  BlockHeader ret;
  ret.setNumber(current_header.number() + 1);
  ret.setParentHash(current_header.hash());
  ret.setAuthor(author);
  ret.setTimestamp(timestamp);
  ret.setGasLimit(current_header.gasLimit());

  BytesMap trxs_trie;
  RLPStream trxs_rlp(transactions.size());
  for (size_t i(0); i < transactions.size(); ++i) {
    auto const& trx_rlp = transactions[i].rlp();
    trxs_trie[rlp(i)] = trx_rlp;
    trxs_rlp.appendRaw(trx_rlp);
  }
  TransactionReceipts receipts;  // TODO
  h256 state_root;               // TODO
  BytesMap receipts_trie;
  RLPStream receipts_rlp(receipts.size());
  LogBloom log_bloom;
  for (size_t i(0); i < receipts.size(); ++i) {
    auto const& receipt = receipts[i];
    auto const& receipt_rlp = receipt.rlp();
    receipts_trie[rlp(i)] = receipt_rlp;
    receipts_rlp.appendRaw(receipt_rlp);
    log_bloom |= receipt.bloom();
  }
  u256 gas_used = receipts.empty() ? 0 : receipts.back().cumulativeGasUsed();
  ret.setGasUsed(gas_used);
  ret.setLogBloom(log_bloom);
  ret.setRoots(hash256(trxs_trie), hash256(receipts_trie), dev::EmptyListSHA3,
               state_root);
  RLPStream block_rlp(3);
  ret.streamRLP(block_rlp);
  block_rlp.appendRaw(trxs_rlp.out());
  static auto const uncles_rlp_list = rlpList();
  block_rlp.appendRaw(uncles_rlp_list);
  auto block_bytes = block_rlp.out();
  auto receipts_bytes = receipts_rlp.out();
  auto _ = {db_adapter_->setBatch(batch), extras_db_adapter_->setBatch(batch)};
  bc_.append_block(block_bytes, &receipts_bytes, u256_zero);
  return ret;
}

Transactions EthService::pendingTransactions() const {
  auto trxs = node_.lock()->getPendingTransactions();
  Transactions ret;
  ret.reserve(trxs.size());
  for (auto const& [_, trx] : trxs) {
    ret.push_back(util::trx_taraxa_2_eth(trx));
  }
  return ret;
}

BlockChain& EthService::bc() { return bc_; }

BlockChain const& EthService::bc() const { return bc_; }

SyncStatus EthService::syncStatus() const {
  // TODO
  return {};
}

BlockHeader EthService::head() const { return bc_.head(); }

}  // namespace taraxa::eth::eth_service