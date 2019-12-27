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

EthService::EthService(shared_ptr<FullNode> const& node,
                       ChainParams const& chain_params,
                       fs::path const& db_base_path,  //
                       milliseconds gc_period,        //
                       WithExisting with_existing,
                       ProgressCallback const& progress_cb)
    : node_(node),
      bc_(chain_params, db_base_path, with_existing, progress_cb),
      acc_state_db_(
          State::openDB(db_base_path, bc_.genesisHash(), with_existing)),
      current_node_account_holder(new FixedAccountHolder(              //
          [this] { return static_cast<dev::eth::Interface*>(this); },  //
          {
              KeyPair(node->getSecretKey()),
          })) {
  assert(chain_params.sealEngineName == TaraxaSealEngine::name());
  assert(chain_params.maxGasLimit <= std::numeric_limits<uint64_t>::max());
  assert(chain_params.gasLimit <= chain_params.maxGasLimit);
  bc_.genesisBlock(acc_state_db_);
  gc_thread_ = thread([this, gc_period] {
    while (!destructor_called_) {
      std::this_thread::sleep_for(gc_period);
      bc_.garbageCollect(true);
    }
  });
}

EthService::~EthService() {
  destructor_called_ = true;
  if (gc_thread_.joinable()) {
    gc_thread_.join();
  }
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
    Address const& author, int64_t timestamp) const {
  auto current_header = getBlockHeader();
  auto number = current_header.number() + 1;
  static bytes const empty_bytes;
  return {
      {
          number,
          current_header.hash(),
          author,
          timestamp,
          current_header.gasLimit(),
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
  LogBloom log_bloom;
  for (size_t i(0); i < receipts.size(); ++i) {
    auto const& receipt = receipts[i];
    auto const& receipt_rlp = receipt.rlp();
    receipts_trie[rlp(i)] = receipt_rlp;
    receipts_rlp.appendRaw(receipt_rlp);
    log_bloom |= receipt.bloom();
  }
  u256 gas_used = receipts.empty() ? 0 : receipts.back().cumulativeGasUsed();
  header.complete(gas_used, log_bloom, hash256(trxs_trie),
                  hash256(receipts_trie), state_root);
  RLPStream block_rlp(3);
  header.streamRLP(block_rlp);
  block_rlp.appendRaw(trxs_rlp.out());
  static auto const uncles_rlp_list = rlpList();
  block_rlp.appendRaw(uncles_rlp_list);
  auto block_bytes = block_rlp.out();
  auto receipts_bytes = receipts_rlp.out();
  // TODO set difficulty to 0 always. Currently each block has difficulty 1
  // because dev::eth::BlockChain requires that total chain difficulty is
  // increasing. This check should be relaxed in the fork.
  // TODO disable validations (to get a speedup) once it's all well-tested
  chain.insertWithoutParent(block_bytes, &receipts_bytes,
                            number * (number + 1) / 2);
  return header;
}

ExecutionResult EthService::call(Address const& _from, u256 _value,
                                 Address _dest, bytes const& _data, u256 _gas,
                                 u256 _gasPrice, BlockNumber _blockNumber,
                                 FudgeFactor _ff) {
  // TODO use taraxa-evm
  auto block = blockByNumber(_blockNumber);
  auto nonce = block.transactionsFrom(_from);
  auto gas =
      _gas == Invalid256 ? sealEngine()->chainParams().maxGasLimit : _gas;
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
  Transactions ret;
  ret.reserve(trxs.size());
  for (auto const& [_, trx] : trxs) {
    ret.push_back(util::trx_taraxa_2_eth(trx));
  }
  return ret;
}

BlockChain& EthService::bc() { return bc_; }

BlockChain const& EthService::bc() const { return bc_; }

Block EthService::block(h256 const& _h) const {
  return Block(bc_, acc_state_db_, getBlockHeader(_h));
}

Block EthService::preSeal() const { return block(bc().currentHash()); }

Block EthService::postSeal() const { return preSeal(); }

SyncStatus EthService::syncStatus() const {
  // TODO
  return {};
}

BlockHeader EthService::getBlockHeader(BlockNumber block_number) const {
  if (block_number == LatestBlock || block_number == PendingBlock) {
    return getBlockHeader(bc_.currentHash());
  }
  return getBlockHeader(bc_.numberHash(block_number));
}

BlockHeader EthService::getBlockHeader(h256 const& hash) const {
  return BlockHeader(bc_.block(hash));
}

State const EthService::getAccountsState(BlockNumber block_number) const {
  return blockByNumber(block_number).state();
}

}  // namespace taraxa::eth::eth_service