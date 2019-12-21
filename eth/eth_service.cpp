#include "eth_service.hpp"

#include <libdevcore/TrieHash.h>
#include <libethashseal/Ethash.h>

#include <stdexcept>

#include "./util.hpp"
#include "full_node.hpp"
#include "taraxa_seal_engine.hpp"

namespace taraxa::eth::eth_service {
using dev::bytesConstRef;
using dev::BytesMap;
using dev::hash256;
using dev::Invalid256;
using dev::rlp;
using dev::rlpList;
using dev::RLPStream;
using dev::eth::Ethash;
using dev::eth::IncludeSeal;
using dev::eth::Permanence;
using dev::eth::VerifiedBlockRef;
using std::move;
using std::unique_lock;
using taraxa_seal_engine::TaraxaSealEngine;

using err = std::runtime_error;

static auto const _ = [] {
  TaraxaSealEngine::init();
  return 0;
}();

EthService::EthService(weak_ptr<FullNode> const& node,
                       ChainParams const& chain_params,
                       fs::path const& db_base_path,  //
                       WithExisting with_existing,
                       ProgressCallback const& progress_cb)
    : node_(node),
      bc_(chain_params, db_base_path, with_existing, progress_cb),
      acc_state_db_(
          State::openDB(db_base_path, bc_.genesisHash(), with_existing)) {
  assert(chain_params.sealEngineName == TaraxaSealEngine::name());
  assert(chain_params.gasLimit <= std::numeric_limits<uint64_t>::max());
  bc_.genesisBlock(acc_state_db_);
}

Address EthService::author() const { return node_.lock()->getAddress(); }

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
  auto ok = node_.lock()->insertTransaction(taraxa_trx);
  if (!ok) throw err("could not insert transaction");
  return taraxa_trx.getHash();
}

pair<PendingBlockHeader, BlockHeader> EthService::startBlock(
    Address const& author, int64_t timestamp) {
  auto current_header = getBlockHeader();
  auto number = current_header.number() + 1;
  static bytes const empty_bytes;
  return {
      {
          number,
          current_header.hash(),
          author,
          timestamp,
          bc().sealEngine()->chainParams().maxGasLimit,
          empty_bytes,
          number,
          h256(0),
          Nonce(0),
      },
      move(current_header),
  };
}

BlockHeader& EthService::commitBlock(PendingBlockHeader& header,
                                     Transactions const& transactions,
                                     TransactionReceipts const& receipts,  //
                                     h256 const& state_root) {
  unique_lock l(append_block_mu_);
  auto& chain = bc();
  auto number = header.number();
  assert(number == chain.number() + 1);
  BytesMap trxs_trie;
  RLPStream trxs_rlp(transactions.size());
  for (size_t i(0); i < transactions.size(); ++i) {
    auto const& trx_rlp = transactions[i].rlp();
    trxs_trie[rlp(i)] = trx_rlp;
    trxs_rlp.appendRaw(trx_rlp);
  }
  BytesMap receipts_trie;
  RLPStream receipts_rlp(receipts.size());
  for (size_t i(0); i < receipts.size(); ++i) {
    auto const& receipt_rlp = receipts[i].rlp();
    receipts_trie[rlp(i)] = receipt_rlp;
    receipts_rlp.appendRaw(receipt_rlp);
  }
  header.complete(hash256(trxs_trie), hash256(receipts_trie), state_root);
  RLPStream block_rlp(3);
  header.streamRLP(block_rlp);
  block_rlp.appendRaw(trxs_rlp.out());
  static auto const uncles_rlp_list = rlpList();
  block_rlp.appendRaw(uncles_rlp_list);
  auto block_bytes = block_rlp.out();
  auto receipts_bytes = receipts_rlp.out();
  // TODO insert pre-verified
  chain.insertWithoutParent(block_bytes, &receipts_bytes,
                            number * (number + 1) / 2);
  return header;
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
  if (_ff == FudgeFactor::Lenient) {
    block.mutableState().addBalance(_from,
                                    u256(t.gas() * t.gasPrice() + t.value()));
  }
  return block.execute(bc().lastBlockHashes(), t, Permanence::Reverted);
}

Transactions EthService::pending() const {
  auto trxs = node_.lock()->getVerifiedTrxSnapShot();
  Transactions ret(trxs.size());
  for (auto const& [_, trx] : trxs) {
    ret.push_back(util::trx_taraxa_2_eth(trx));
  }
  return ret;
}

BlockChain& EthService::bc() { return bc_; }

BlockChain const& EthService::bc() const { return bc_; }

Block EthService::block(h256 const& _h) const {
  BlockHeader header(bc_.block(_h));
  return Block(bc_, acc_state_db_, header.stateRoot(), header.author());
}

Block EthService::preSeal() const { return block(bc().currentHash()); }

Block EthService::postSeal() const { return preSeal(); }

SyncStatus EthService::syncStatus() const {
  // TODO
  return {};
}

}  // namespace taraxa::eth::eth_service