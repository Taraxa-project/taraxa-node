#pragma once

#include <chrono>
#include <deque>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#include "ChainDB.h"
#include "LogFilter.h"
#include "TrieCommon.h"
#include "storage/db_storage.hpp"
#include "util/encoding_rlp.hpp"

namespace taraxa::final_chain {

std::string const LAST_BLOCK_KEY("final_chain_last_block_hash");

struct ChainDBImpl : virtual ChainDB {
  mutable std::shared_mutex last_block_mu_;
  mutable std::shared_ptr<BlockHeader> last_block_;

  std::shared_ptr<DbStorage> db_;

 public:
  ChainDBImpl(decltype(db_) db) : db_(std::move(db)) {
    if (auto hash_str = db_->lookup(DbStorage::Columns::default_column, LAST_BLOCK_KEY); !hash_str.empty()) {
      //      last_block_ = std::make_shared<BlockHeader>(blockHeader(h256(hash_str, h256::FromBinary)));
    }
  }
  virtual ~ChainDBImpl(){};

  BlockNumber last_block_number() const {  // TODO
    return 0;
  }

  optional<BlockHeaderWithTransactions> blockHeaderWithTransactions(BlockNumber n,
                                                                    bool full_transactions) const override {
    return std::optional<BlockHeaderWithTransactions>();
  }

  optional<BlockHeaderWithTransactions> blockHeaderWithTransactions(h256 const& _hash,
                                                                    bool full_transactions) const override {
    return std::optional<BlockHeaderWithTransactions>();
  }

  LocalisedLogEntries logs(LogFilter const& filter) const override {
    LocalisedLogEntries ret;
    auto latest_blk_num = last_block_number();
    auto begin = min(latest_blk_num + 1, filter.latest());
    auto end = min(latest_blk_num, min(begin, filter.earliest()));

    if (begin > latest_blk_num) {
      begin = latest_blk_num;
    }

    // Handle reverted blocks
    // There are not so many, so let's iterate over them
    h256s blocks;
    h256 ancestor;
    unsigned ancestorIndex;
    //    tie(blocks, ancestor, ancestorIndex) = treeRoute(hashFromNumber(begin), hashFromNumber(end), false); TODO

    for (size_t i = 0; i < ancestorIndex; i++) {
      prependLogsFromBlock(filter, blocks[i], ret);
    }

    // cause end is our earliest block, let's compare it with our ancestor
    // if ancestor is smaller let's move our end to it
    // example:
    //
    // 3b -> 2b -> 1b
    //                -> g
    // 3a -> 2a -> 1a
    //
    // if earliest is at 2a and latest is a 3b, coverting them to numbers
    // will give us pair (2, 3)
    // and we want to get all logs from 1 (ancestor + 1) to 3
    // so we have to move 2a to g + 1
    //    end = min(end, numberFromHash(ancestor) + 1); TODO

    // Handle blocks from main chain
    set<unsigned> matchingBlocks;
    if (!filter.isRangeFilter()) {
      for (auto const& i : filter.bloomPossibilities()) {
        for (auto u : withBlockBloom(i, end, begin)) {
          matchingBlocks.insert(u);
        }
      }
    } else {
      // if it is a range filter, we want to get all logs from all blocks in given
      // range
      for (unsigned i = end; i <= begin; i++) {
        matchingBlocks.insert(i);
      }
    }

    for (auto n : matchingBlocks) {
      prependLogsFromBlock(filter, hashFromNumber(n), ret);
    }

    reverse(ret.begin(), ret.end());
    return ret;
  }

  optional<LocalisedTransaction> localisedTransaction(unsigned int _i, BlockNumber n) const override {
    return std::optional<LocalisedTransaction>();
  }

  optional<LocalisedTransaction> localisedTransaction(h256 const& _blockHash, unsigned int _i) const override {
    return std::optional<LocalisedTransaction>();
  }

  optional<LocalisedTransaction> localisedTransaction(h256 const& _transactionHash) const override {
    return std::optional<LocalisedTransaction>();
  }

  optional<LocalisedTransactionReceipt> localisedTransactionReceipt(h256 const& _transactionHash) const override {
    return std::optional<LocalisedTransactionReceipt>();
  }

  unsigned int transactionCount(BlockNumber n) const override { return 0; }

  unsigned int transactionCount(h256 const& _blockHash) const override { return 0; }

  h256 hashFromNumber(BlockNumber _i) const {
    auto s = db_->lookup(DbStorage::Columns::final_chain_block_number_to_hash, _i);
    return s.empty() ? h256() : h256(s, h256::FromBinary);
  }

  BlockHeader blockHeader(BlockNumber n) const {}

  Transactions transactions(BlockNumber n) const {}

  static inline unsigned upow(unsigned a, unsigned b) {
    if (!b) {
      return 1;
    }
    while (--b > 0) {
      a *= a;
    }
    return a;
  }

  static inline unsigned ceilDiv(unsigned n, unsigned d) { return (n + d - 1) / d; }

  // Level 1
  // [xxx.            ]

  // Level 0
  // [.x............F.]
  // [........x.......]
  // [T.............x.]
  // [............    ]

  // F = 14. T = 32

  vector<BlockNumber> withBlockBloom(LogBloom const& _b, BlockNumber _earliest, BlockNumber _latest) const {
    vector<BlockNumber> ret;
    // start from the top-level
    auto u = upow(c_bloomIndexSize, c_bloomIndexLevels);
    // run through each of the top-level blockbloom blocks
    for (auto index = _earliest / u; index <= ceilDiv(_latest, u); ++index) {  // 0
      ::dev::operator+=(ret, withBlockBloom(_b, _earliest, _latest, c_bloomIndexLevels - 1, index));
    }
    return ret;
  }

  vector<BlockNumber> withBlockBloom(LogBloom const& _b, BlockNumber _earliest, BlockNumber _latest, unsigned _level,
                                     unsigned _index) const {
    // 14, 32, 1, 0
    // 14, 32, 0, 0
    // 14, 32, 0, 1
    // 14, 32, 0, 2

    vector<BlockNumber> ret;

    auto uCourse = upow(c_bloomIndexSize, _level + 1);
    // 256
    // 16
    auto uFine = upow(c_bloomIndexSize, _level);
    // 16
    // 1

    auto obegin = _index == _earliest / uCourse ? _earliest / uFine % c_bloomIndexSize : 0;
    // 0
    // 14
    // 0
    // 0
    auto oend = _index == _latest / uCourse ? (_latest / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
    // 3
    // 16
    // 16
    // 1

    auto bb = blocksBlooms(_level, _index);
    for (auto o = obegin; o < oend; ++o)
      if (bb[o].contains(_b)) {
        // This level has something like what we want.
        if (_level > 0) {
          ::dev::operator+=(ret, withBlockBloom(_b, _earliest, _latest, _level - 1, o + _index * c_bloomIndexSize));
        } else {
          ret.push_back(o + _index * c_bloomIndexSize);
        }
      }
    return ret;
  }

  void prependLogsFromBlock(LogFilter const& _f, h256 const& _blockHash, LocalisedLogEntries& io_logs) const {
    //    TODO
    //    auto receipts_ = receipts(_blockHash);
    //    auto blk_n = numberFromHash(_blockHash);
    //    for (uint trx_i = 0; trx_i < receipts_.size(); trx_i++) {
    //      auto const& receipt = receipts_[trx_i];
    //      ExtendedTransactionLocation trx_loc{
    //          _blockHash,
    //          trx_i,
    //          blk_n,
    //          transaction(_blockHash, trx_i).getHash(),
    //      };
    //      auto prepend = [&](uint log_i) {
    //        io_logs.insert(io_logs.begin(), LocalisedLogEntry{
    //                                            receipt.m_log[log_i],
    //                                            trx_loc,
    //                                            log_i,
    //                                        });
    //      };
    //      auto match_result = _f.matches(receipt);
    //      if (match_result.all_match) {
    //        for (uint log_i = 0; log_i < receipt.m_log.size(); ++log_i) {
    //          prepend(log_i);
    //        }
    //        continue;
    //      }
    //      for (auto log_i : match_result.match_positions) {
    //        prepend(log_i);
    //      }
    //    }
  }

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

  BlocksBlooms blocksBlooms(h256 const& chunk_id) const {
    db_->lookup(DbStorage::Columns::final_chain_log_blooms_index, chunk_id);
  }

  static h256 chunkId(unsigned _level, unsigned _index) { return h256(_index * 0xff + _level); }
};

}  // namespace taraxa::final_chain
