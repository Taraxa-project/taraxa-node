#include "eth_service.hpp"

#include <libdevcore/TrieHash.h>
#include <libethashseal/Ethash.h>

#include <stdexcept>

#include "./util.hpp"

namespace taraxa::eth::eth_service {
using dev::bytesConstRef;
using dev::BytesMap;
using dev::hash256;
using dev::Invalid256;
using dev::rlp;
using dev::RLPStream;
using dev::eth::Ethash;
using dev::eth::IncludeSeal;
using dev::eth::Permanence;
using dev::eth::VerifiedBlockRef;
using std::unique_lock;

using err = std::runtime_error;

EthService::EthService(shared_ptr<FullNode> const& node,
                       ChainParams const& chain_params,
                       fs::path const& db_base_path, WithExisting with_existing,
                       ProgressCallback const& progress_cb)
    : node_(node),
      bc_(chain_params, db_base_path, with_existing, progress_cb),
      acc_state_db_(
          State::openDB(db_base_path, bc_.genesisHash(), with_existing)) {
  bc_.genesisBlock(acc_state_db_);
}

Address EthService::author() const { return node_->getAddress(); }

TransactionSkeleton EthService::populateTransactionWithDefaults(
    TransactionSkeleton const& _t) const {
  TransactionSkeleton ret(_t);
  //  https://github.com/ethereum/wiki/wiki/JSON-RPC#eth_sendtransaction
  static const u256 defaultTransactionGas = 90000;
  if (ret.nonce == Invalid256) {
    ret.nonce = postSeal().transactionsFrom(ret.from);
  }
  if (ret.gasPrice == Invalid256) {
    ret.gasPrice = gasBidPrice();
  }
  if (ret.gas == Invalid256) {
    ret.gas = defaultTransactionGas;
  }
  return ret;
}

h256 EthService::submitTransaction(TransactionSkeleton const& _t,
                                   Secret const& _secret) {
  return importTransaction({populateTransactionWithDefaults(_t), _secret});
}

h256 EthService::importTransaction(Transaction const& _t) {
  auto taraxa_trx = util::trx_eth_2_taraxa(_t);
  auto ok = node_->insertTransaction(taraxa_trx);
  if (!ok) throw err("could not insert transaction");
  return taraxa_trx.getHash();
}

void EthService::appendBlock(Transactions const& transactions,
                             TransactionReceipts const& receipts,  //
                             h256 state_root,                      //
                             int64_t timestamp,                    //
                             Address author) {
  unique_lock l(append_block_mu_);
  auto& chain = bc();
  BlockHeader header;
  header.setNumber(chain.number());
  header.setParentHash(chain.currentHash());
  header.setAuthor(author);
  header.setTimestamp(timestamp);
  BytesMap transaction_trie;
  for (size_t i(0); i < transactions.size(); ++i) {
    transaction_trie[rlp(i)] = transactions[i].rlp();
  }
  BytesMap receipts_trie;
  RLPStream receipts_rlp(receipts.size());
  for (size_t i(0); i < receipts.size(); ++i) {
    auto const& receipt = receipts[i];
    receipts_trie[rlp(i)] = receipt.rlp();
    receipt.streamRLP(receipts_rlp);
  }
  bytes receipts_bytes = receipts_rlp.out();
  header.setRoots(hash256(transaction_trie), hash256(receipts_trie),
                  dev::EmptyListSHA3, state_root);
  header.setGasLimit(MOCK_BLOCK_GAS_LIMIT);
  //  Ethash::setMixHash(block_header, 0);
  //  Ethash::setNonce(block_header, 0);
  //  block_header.setDifficulty(0);
  RLPStream header_rlp;
  header.streamRLP(header_rlp);
  bytes header_bytes = header_rlp.out();
  chain.insert(
      VerifiedBlockRef{
          bytesConstRef(const_cast<::byte const*>(header_bytes.data()),
                        header_bytes.size()),
          header,
          transactions,
      },
      bytesConstRef(const_cast<::byte const*>(receipts_bytes.data()),
                    receipts_bytes.size()));
}

ExecutionResult EthService::call(Address const& _from, u256 _value,
                                 Address _dest, bytes const& _data, u256 _gas,
                                 u256 _gasPrice, BlockNumber _blockNumber,
                                 FudgeFactor _ff) {
  auto block = blockByNumber(_blockNumber);
  auto nonce = block.transactionsFrom(_from);
  auto gas = _gas == Invalid256 ? block.gasLimitRemaining() : _gas;
  auto gasPrice = _gasPrice == Invalid256 ? 0 : _gasPrice;
  Transaction t(_value, gasPrice, gas, _dest, _data, nonce);
  t.forceSender(_from);
  if (_ff == FudgeFactor::Lenient)
    block.mutableState().addBalance(_from,
                                    u256(t.gas() * t.gasPrice() + t.value()));
  return block.execute(bc().lastBlockHashes(), t, Permanence::Reverted);
}

Transactions EthService::pending() const {
  auto trxs = node_->getVerifiedTrxSnapShot();
  Transactions ret(trxs.size());
  for (auto const& [_, trx] : node_->getVerifiedTrxSnapShot()) {
    ret.push_back(util::trx_taraxa_2_eth(trx));
  }
  return ret;
}

BlockChain& EthService::bc() { return bc_; }

BlockChain const& EthService::bc() const { return bc_; }

Block EthService::block(h256 const& _h) const {
  Block ret(bc_, acc_state_db_);
  ret.populateFromChain(bc_, _h);
  return ret;
}

Block EthService::preSeal() const { return block(bc().currentHash()); }

Block EthService::postSeal() const { return preSeal(); }

SyncStatus EthService::syncStatus() const {
  // TODO
  return {};
}

}  // namespace taraxa::eth::eth_service