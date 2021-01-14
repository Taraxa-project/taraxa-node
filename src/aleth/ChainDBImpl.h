#pragma once

#include <boost/filesystem/path.hpp>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "BlockDetails.h"
#include "BlockHeader.h"
#include "ChainDB.h"
#include "LocalizedTransaction.h"
#include "LogEntry.h"
#include "LogFilter.h"
#include "TransactionReceipt.h"
#include "db_storage.hpp"

namespace taraxa::aleth {

class ChainDBImpl : virtual ChainDB {
  mutable boost::shared_mutex last_block_mu_;
  mutable std::shared_ptr<BlockHeader> last_block_;

  std::shared_ptr<DbStorage> db_;

 public:
  ChainDBImpl(decltype(db_) db);

  BlockHeader append_block(DbStorage::Batch& b, Address const& author, uint64_t timestamp, uint64_t gas_limit,
                           h256 const& state_root, Transactions const& transactions,
                           TransactionReceipts const& receipts);
  void refresh_last_block();

  auto get_last_block() const {
    dev::ReadGuard l(last_block_mu_);
    return last_block_;
  }
  BlockNumber last_block_number() const { return get_last_block()->number(); }
  /// Get the hash for a given block's number.
  h256 hashFromNumber(BlockNumber _i) const {
    return queryExtras<BlockHash>(DbStorage::Columns::final_chain_block_number_to_hash, _i).value;
  }
  /// Returns true if the given block is known (though not necessarily a part of
  /// the canon chain).
  bool isKnown(BlockNumber _block) const;
  bool isKnown(h256 const& _hash) const;
  /// Get the partial-header of a block (or the most recent mined if none
  /// given). Thread-safe.
  BlockHeader blockHeader(BlockNumber n) const { return blockHeader(hashFromNumber(n)); }
  BlockHeader blockHeader(h256 const& _hash) const { return BlockHeader(dev::RLP(block(_hash))[0]); }
  /// Get the familial details concerning a block (or the most recent mined if
  /// none given). Thread-safe.
  BlockDetails blockDetails(BlockNumber n) const { return blockDetails(hashFromNumber(n)); }
  BlockDetails blockDetails(h256 const& _hash) const {
    return queryExtras<BlockDetails>(DbStorage::Columns::final_chain_block_details, _hash);
  }

  /// Get the transactions' receipts of a block (or the most recent mined if
  /// none given). Thread-safe. receipts are given in the same order are in the
  /// same order as the transactions
  BlockReceipts receipts(h256 const& _hash) const {
    return queryExtras<BlockReceipts>(DbStorage::Columns::final_chain_receipts, _hash);
  }
  /// Get the transaction by block hash and index;
  TransactionReceipt transactionReceipt(h256 const& _blockHash, unsigned _i) const {
    return receipts(_blockHash).receipts[_i];
  }
  /// Get the transaction receipt by transaction hash. Thread-safe.
  TransactionReceipt transactionReceipt(h256 const& _transactionHash) const {
    auto ta = queryExtras<TransactionLocation>(DbStorage::Columns::final_chain_transaction_locations, _transactionHash);
    if (!ta) return bytesConstRef();
    return transactionReceipt(ta.blockHash, ta.index);
  }

  /// Returns true if transaction is known. Thread-safe
  bool isKnownTransaction(h256 const& _transactionHash) const {
    return !!queryExtras<TransactionLocation>(DbStorage::Columns::final_chain_transaction_locations, _transactionHash);
  }
  bool isKnownTransaction(unsigned _i, BlockNumber n) const { return isKnownTransaction(hashFromNumber(n), _i); }
  bool isKnownTransaction(h256 const& _blockHash, unsigned _i) const;

  LocalisedLogEntries logs(LogFilter const& _filter) const;
  Transaction transaction(h256 _transactionHash) const;
  Transaction transaction(h256 _blockHash, unsigned _i) const;

  LocalisedTransaction localisedTransaction(unsigned _i, BlockNumber n) const {
    return localisedTransaction(hashFromNumber(n), _i);
  }
  LocalisedTransaction localisedTransaction(h256 const& _blockHash, unsigned _i) const;
  LocalisedTransaction localisedTransaction(h256 const& _transactionHash) const;

  LocalisedTransactionReceipt localisedTransactionReceipt(h256 const& _transactionHash) const;

  Transactions transactions(BlockNumber n) const { return transactions(hashFromNumber(n)); }
  Transactions transactions(h256 _blockHash) const;

  h256s transactionHashes(BlockNumber n) const { return transactionHashes(hashFromNumber(n)); }
  /// Get a list of transaction hashes for a given block. Thread-safe.
  h256s transactionHashes(h256 const& _hash) const {
    auto blk = block(_hash);
    dev::RLP rlp(blk);
    h256s ret;
    for (auto t : rlp[1]) ret.push_back(sha3(t.data()));
    return ret;
  }

  unsigned transactionCount(BlockNumber n) const { return transactionCount(hashFromNumber(n)); }
  unsigned transactionCount(h256 const& _blockHash) const;

 private:
  BlockNumber numberFromHash(h256 _blockHash) const { return blockDetails(_blockHash).number; }
  /// Get a block (RLP format) for the given hash (or the most recent mined if
  /// none given). Thread-safe.
  std::string block(h256 const& _hash) const;
  /** @returns a tuple of:
   * - an vector of hashes of all blocks between @a _from and @a _to, all blocks
   * are ordered first by a number of blocks that are parent-to-child, then two
   * sibling blocks, then a number of blocks that are child-to-parent;
   * - the block hash of the latest common ancestor of both blocks;
   * - the index where the latest common ancestor of both blocks would either be
   * found or inserted, depending on whether it is included.
   *
   * @param _common if true, include the common ancestor in the returned vector.
   * @param _pre if true, include all block hashes running from @a _from until
   * the common ancestor in the returned vector.
   * @param _post if true, include all block hashes running from the common
   * ancestor until @a _to in the returned vector.
   *
   * e.g. if the block tree is 3a -> 2a -> 1a -> g and 2b -> 1b -> g (g is
   * genesis, *a, *b are competing chains), then:
   * @code
   * treeRoute(3a, 2b, false) == make_tuple({ 3a, 2a, 1a, 1b, 2b }, g, 3);
   * treeRoute(2a, 1a, false) == make_tuple({ 2a, 1a }, 1a, 1)
   * treeRoute(1a, 2a, false) == make_tuple({ 1a, 2a }, 1a, 0)
   * treeRoute(1b, 2a, false) == make_tuple({ 1b, 1a, 2a }, g, 1)
   * treeRoute(3a, 2b, true) == make_tuple({ 3a, 2a, 1a, g, 1b, 2b }, g, 3);
   * treeRoute(2a, 1a, true) == make_tuple({ 2a, 1a }, 1a, 1)
   * treeRoute(1a, 2a, true) == make_tuple({ 1a, 2a }, 1a, 0)
   * treeRoute(1b, 2a, true) == make_tuple({ 1b, g, 1a, 2a }, g, 1)
   * @endcode
   */
  std::tuple<h256s, h256, unsigned> treeRoute(h256 const& _from, h256 const& _to, bool _common = true, bool _pre = true,
                                              bool _post = true) const;
  /** Get the block blooms for a number of blocks. Thread-safe.
   * @returns the object pertaining to the blocks:
   * level 0:
   * 0x, 0x + 1, .. (1x - 1)
   * 1x, 1x + 1, .. (2x - 1)
   * ...
   * (255x .. (256x - 1))
   * level 1:
   * 0x .. (1x - 1), 1x .. (2x - 1), ..., (255x .. (256x - 1))
   * 256x .. (257x - 1), 257x .. (258x - 1), ..., (511x .. (512x - 1))
   * ...
   * level n, index i, offset o:
   * i * (x ^ n) + o * x ^ (n - 1)
   */
  BlocksBlooms blocksBlooms(unsigned _level, unsigned _index) const { return blocksBlooms(chunkId(_level, _index)); }
  BlocksBlooms blocksBlooms(h256 const& _chunkId) const {
    return queryExtras<BlocksBlooms>(DbStorage::Columns::final_chain_log_blooms_index, _chunkId);
  }
  std::vector<unsigned> withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest) const;
  std::vector<unsigned> withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest, unsigned _topLevel,
                                       unsigned _index) const;
  static h256 chunkId(unsigned _level, unsigned _index) { return h256(_index * 0xff + _level); }
  /// Get the transactions' log blooms of a block (or the most recent mined if
  /// none given). Thread-safe.
  BlockLogBlooms logBlooms(h256 const& _hash) const {
    return queryExtras<BlockLogBlooms>(DbStorage::Columns::final_chain_log_blooms_index, _hash);
  }
  void prependLogsFromBlock(LogFilter const& _filter, h256 const& _blockHash, LocalisedLogEntries& io_logs) const;

  /// Get a block's transaction (RLP format) for the given block hash (or the
  /// most recent mined if none given) & index. Thread-safe.
  bytes transaction_raw(h256 const& _blockHash, unsigned _i) const {
    return dev::RLP(block(_blockHash))[1][_i].data().toBytes();
  }
  /// Get a transaction from its hash. Thread-safe.
  bytes transaction_raw(h256 const& _transactionHash) const {
    auto ta = queryExtras<TransactionLocation>(DbStorage::Columns::final_chain_transaction_locations, _transactionHash);
    if (!ta) return bytes();
    return transaction_raw(ta.blockHash, ta.index);
  }
  std::pair<h256, unsigned> transactionLocation(h256 const& _transactionHash) const {
    auto ta = queryExtras<TransactionLocation>(DbStorage::Columns::final_chain_transaction_locations, _transactionHash);
    if (!ta) return std::pair<h256, unsigned>(h256(), 0);
    return std::make_pair(ta.blockHash, ta.index);
  }

  template <class T, class K>
  T queryExtras(DbStorage::Column const& col, K const& k) const {
    auto s = db_->lookup(k, col);
    if (s.empty()) return T();
    return T(dev::RLP(s));
  }
};

}  // namespace taraxa::aleth
