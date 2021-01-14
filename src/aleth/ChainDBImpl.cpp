#include "ChainDBImpl.h"

#include <libdevcore/Assertions.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/RLP.h>

#include "BlockHeader.h"
#include "Exceptions.h"
#include "TrieHash.h"

namespace taraxa::aleth {
using namespace ::std;
using namespace ::dev;

std::string const LAST_BLOCK_KEY("final_chain_last_block_hash");

ChainDBImpl::ChainDBImpl(decltype(db_) db) : db_(std::move(db)) { refresh_last_block(); }

void ChainDBImpl::refresh_last_block() {
  if (auto h = db_->lookup(LAST_BLOCK_KEY, DbStorage::Columns::default_column); !h.empty()) {
    WriteGuard l(last_block_mu_);
    auto hash = h256(h, h256::FromBinary);
    last_block_ = std::make_shared<BlockHeader>(blockHeader(hash));
  }
}

BlockHeader ChainDBImpl::append_block(DbStorage::Batch& b, Address const& author, uint64_t timestamp,
                                      uint64_t gas_limit, h256 const& state_root, Transactions const& transactions,
                                      TransactionReceipts const& receipts) {
  BlockHeaderFields header;
  auto last_block = get_last_block();
  header.m_number = last_block ? last_block->number() + 1 : 0;
  header.m_parentHash = last_block ? last_block->hash() : h256();
  header.m_author = author;
  header.m_timestamp = timestamp;
  header.m_stateRoot = state_root;
  header.m_gasUsed = receipts.empty() ? 0 : receipts.back().cumulativeGasUsed();
  header.m_gasLimit = gas_limit;
  header.m_sha3Uncles = EmptyListSHA3;
  BytesMap trxs_trie;
  RLPStream trxs_rlp(transactions.size());
  BytesMap receipts_trie;
  RLPStream receipts_rlp(transactions.size());
  BlockLogBlooms blb;
  blb.blooms.reserve(transactions.size());
  for (size_t i(0); i < transactions.size(); ++i) {
    trxs_rlp.appendRaw(trxs_trie[rlp(i)] = *transactions[i].rlp());
    auto const& receipt = receipts[i];
    receipts_rlp.appendRaw(receipts_trie[rlp(i)] = receipt.rlp());
    header.m_logBloom |= receipt.bloom();
    blb.blooms.push_back(receipt.bloom());
  }
  header.m_transactionsRoot = hash256(trxs_trie);
  header.m_receiptsRoot = hash256(receipts_trie);
  RLPStream block_rlp_stream(2);
  header.streamRLP(block_rlp_stream);
  block_rlp_stream.appendRaw(trxs_rlp.out());
  auto const& block_bytes = block_rlp_stream.out();
  auto const& header_bytes = RLP(block_bytes)[0].data();
  auto const& blk_hash = sha3(header_bytes);
  db_->batch_put(b, DbStorage::Columns::final_chain_blocks, blk_hash, block_bytes);
  BlockDetails details(header.m_number, 0, header.m_parentHash, header_bytes.size());
  db_->batch_put(b, DbStorage::Columns::final_chain_block_details, blk_hash, details.rlp());
  db_->batch_put(b, DbStorage::Columns::final_chain_log_blooms, blk_hash, blb.rlp());
  db_->batch_put(b, DbStorage::Columns::final_chain_receipts, blk_hash, receipts_rlp.out());
  header.m_logBloom.shiftBloom<3>(sha3(header.m_author.ref()));
  for (uint64_t level = 0, index = header.m_number; level < c_bloomIndexLevels; level++, index /= c_bloomIndexSize) {
    auto h = chunkId(level, index / c_bloomIndexSize);
    auto bb = blocksBlooms(h);
    bb.blooms[index % c_bloomIndexSize] |= header.m_logBloom;
    db_->batch_put(b, DbStorage::Columns::final_chain_log_blooms_index, h, bb.rlp());
  }
  TransactionLocation ta;
  ta.blockHash = blk_hash;
  for (auto const& trx : transactions) {
    db_->batch_put(b, DbStorage::Columns::final_chain_transaction_locations, trx.getHash(), ta.rlp());
    ta.index++;
  }
  db_->batch_put(b, DbStorage::Columns::final_chain_block_number_to_hash, header.m_number, BlockHash(blk_hash).rlp());
  db_->batch_put(b, DbStorage::Columns::default_column, LAST_BLOCK_KEY, blk_hash);
  return {header, blk_hash};
}

tuple<h256s, h256, unsigned> ChainDBImpl::treeRoute(h256 const& _from, h256 const& _to, bool _common, bool _pre,
                                                    bool _post) const {
  if (!_from || !_to) return make_tuple(h256s(), h256(), 0);

  BlockDetails const fromDetails = blockDetails(_from);
  BlockDetails const toDetails = blockDetails(_to);
  // Needed to handle a special case when the parent of inserted block is not
  // present in DB.
  if (fromDetails.isNull() || toDetails.isNull()) return make_tuple(h256s(), h256(), 0);

  unsigned fn = fromDetails.number;
  unsigned tn = toDetails.number;
  h256s ret;
  h256 from = _from;
  while (fn > tn) {
    if (_pre) ret.push_back(from);
    from = blockDetails(from).parent;
    fn--;
  }

  h256s back;
  h256 to = _to;
  while (fn < tn) {
    if (_post) back.push_back(to);
    to = blockDetails(to).parent;
    tn--;
  }
  for (;; from = blockDetails(from).parent, to = blockDetails(to).parent) {
    if (_pre && (from != to || _common)) ret.push_back(from);
    if (_post && (from != to || (!_pre && _common))) back.push_back(to);

    if (from == to) break;
    if (!from) assert(from);
    if (!to) assert(to);
  }
  ret.reserve(ret.size() + back.size());
  unsigned i = ret.size() - (int)(_common && !ret.empty() && !back.empty());
  for (auto it = back.rbegin(); it != back.rend(); ++it) ret.push_back(*it);
  return make_tuple(ret, from, i);
}

template <class K, class T>
static unsigned getHashSize(unordered_map<K, T> const& _map) {
  unsigned ret = 0;
  for (auto const& i : _map) ret += i.second.size + 64;
  return ret;
}

static inline unsigned upow(unsigned a, unsigned b) {
  if (!b) return 1;
  while (--b > 0) a *= a;
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

vector<unsigned> ChainDBImpl::withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest) const {
  vector<unsigned> ret;

  // start from the top-level
  unsigned u = upow(c_bloomIndexSize, c_bloomIndexLevels);

  // run through each of the top-level blockbloom blocks
  for (unsigned index = _earliest / u; index <= ceilDiv(_latest, u); ++index)  // 0
    ::dev::operator+=(ret, withBlockBloom(_b, _earliest, _latest, c_bloomIndexLevels - 1, index));

  return ret;
}

vector<unsigned> ChainDBImpl::withBlockBloom(LogBloom const& _b, unsigned _earliest, unsigned _latest, unsigned _level,
                                             unsigned _index) const {
  // 14, 32, 1, 0
  // 14, 32, 0, 0
  // 14, 32, 0, 1
  // 14, 32, 0, 2

  vector<unsigned> ret;

  unsigned uCourse = upow(c_bloomIndexSize, _level + 1);
  // 256
  // 16
  unsigned uFine = upow(c_bloomIndexSize, _level);
  // 16
  // 1

  unsigned obegin = _index == _earliest / uCourse ? _earliest / uFine % c_bloomIndexSize : 0;
  // 0
  // 14
  // 0
  // 0
  unsigned oend = _index == _latest / uCourse ? (_latest / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
  // 3
  // 16
  // 16
  // 1

  BlocksBlooms bb = blocksBlooms(_level, _index);
  for (unsigned o = obegin; o < oend; ++o)
    if (bb.blooms[o].contains(_b)) {
      // This level has something like what we want.
      if (_level > 0)
        ::dev::operator+=(ret, withBlockBloom(_b, _earliest, _latest, _level - 1, o + _index * c_bloomIndexSize));
      else
        ret.push_back(o + _index * c_bloomIndexSize);
    }
  return ret;
}

bool ChainDBImpl::isKnown(h256 const& _hash) const {
  return !db_->lookup(_hash, DbStorage::Columns::final_chain_blocks).empty();
}

std::string ChainDBImpl::block(h256 const& _hash) const {
  return db_->lookup(_hash, DbStorage::Columns::final_chain_blocks);
}

LocalisedLogEntries ChainDBImpl::logs(LogFilter const& _f) const {
  LocalisedLogEntries ret;
  auto latest_blk_num = last_block_number();
  auto begin = min(latest_blk_num + 1, _f.latest());
  auto end = min(latest_blk_num, min(begin, _f.earliest()));

  if (begin > latest_blk_num) {
    begin = latest_blk_num;
  }

  // Handle reverted blocks
  // There are not so many, so let's iterate over them
  h256s blocks;
  h256 ancestor;
  unsigned ancestorIndex;
  tie(blocks, ancestor, ancestorIndex) = treeRoute(hashFromNumber(begin), hashFromNumber(end), false);

  for (size_t i = 0; i < ancestorIndex; i++) prependLogsFromBlock(_f, blocks[i], ret);

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
  end = min(end, numberFromHash(ancestor) + 1);

  // Handle blocks from main chain
  set<unsigned> matchingBlocks;
  if (!_f.isRangeFilter())
    for (auto const& i : _f.bloomPossibilities())
      for (auto u : withBlockBloom(i, end, begin)) matchingBlocks.insert(u);
  else
    // if it is a range filter, we want to get all logs from all blocks in given
    // range
    for (unsigned i = end; i <= begin; i++) matchingBlocks.insert(i);

  for (auto n : matchingBlocks) prependLogsFromBlock(_f, hashFromNumber(n), ret);

  reverse(ret.begin(), ret.end());
  return ret;
}

void ChainDBImpl::prependLogsFromBlock(LogFilter const& _f, h256 const& _blockHash,
                                       LocalisedLogEntries& io_logs) const {
  auto receipts_ = receipts(_blockHash).receipts;
  for (size_t i = 0; i < receipts_.size(); i++) {
    TransactionReceipt receipt = receipts_[i];
    auto th = transaction(_blockHash, i).getHash();
    LogEntries le = _f.matches(receipt);
    for (unsigned j = 0; j < le.size(); ++j)
      io_logs.insert(io_logs.begin(), LocalisedLogEntry(le[j], _blockHash, numberFromHash(_blockHash), th, i, 0));
  }
}

Transaction ChainDBImpl::transaction(h256 _transactionHash) const {
  return Transaction(transaction_raw(_transactionHash));
}

LocalisedTransaction ChainDBImpl::localisedTransaction(h256 const& _transactionHash) const {
  std::pair<h256, unsigned> tl = transactionLocation(_transactionHash);
  return localisedTransaction(tl.first, tl.second);
}

unsigned ChainDBImpl::transactionCount(h256 const& _blockHash) const {
  if (auto b = block(_blockHash); !b.empty()) {
    return RLP(b)[1].itemCount();
  }
  return 0;
}

Transaction ChainDBImpl::transaction(h256 _blockHash, unsigned _i) const {
  auto bl = block(_blockHash);
  RLP b(bl);
  if (_i < b[1].itemCount())
    return Transaction(dev::RLP(b[1][_i].data()));
  else
    return Transaction();
}

LocalisedTransaction ChainDBImpl::localisedTransaction(h256 const& _blockHash, unsigned _i) const {
  Transaction t = Transaction(transaction_raw(_blockHash, _i));
  return LocalisedTransaction(t, _blockHash, _i, numberFromHash(_blockHash));
}

LocalisedTransactionReceipt ChainDBImpl::localisedTransactionReceipt(h256 const& _transactionHash) const {
  std::pair<h256, unsigned> tl = transactionLocation(_transactionHash);
  Transaction t = Transaction(transaction_raw(tl.first, tl.second));
  TransactionReceipt tr = transactionReceipt(tl.first, tl.second);
  u256 gasUsed = tr.cumulativeGasUsed();
  if (tl.second > 0) gasUsed -= transactionReceipt(tl.first, tl.second - 1).cumulativeGasUsed();
  return LocalisedTransactionReceipt(tr, t.getHash(), tl.first, numberFromHash(tl.first), t.getSender(),
                                     t.getReceiver().value_or(ZeroAddress), tl.second, gasUsed);
}

Transactions ChainDBImpl::transactions(h256 _blockHash) const {
  auto bl = block(_blockHash);
  RLP b(bl);
  Transactions res;
  for (unsigned i = 0; i < b[1].itemCount(); i++) res.emplace_back(RLP(b[1][i].data()));
  return res;
}

bool ChainDBImpl::isKnown(BlockNumber _block) const { return hashFromNumber(_block) != h256(); }

bool ChainDBImpl::isKnownTransaction(h256 const& _blockHash, unsigned _i) const {
  return ChainDBImpl::isKnown(_blockHash) && transactions(_blockHash).size() > _i;
}

}  // namespace taraxa::aleth