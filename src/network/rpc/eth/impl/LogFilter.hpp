#pragma once

#include "data.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::net::rpc::eth {

struct LogFilter {
  using Topics = array<unordered_set<h256>, 4>;

 private:
  optional<BlockNumber> from_block_;
  optional<BlockNumber> to_block_;
  AddressSet addresses_;
  Topics topics_;
  bool is_range_only_ = false;

 public:
  LogFilter(decltype(from_block_) from_block, decltype(to_block_) to_block, decltype(addresses_) addresses,
            decltype(topics_) topics)
      : from_block_(move(from_block)), to_block_(move(to_block)), addresses_(move(addresses)), topics_(move(topics)) {
    if (!addresses_.empty()) {
      return;
    }
    for (auto const& t : topics_) {
      if (!t.empty()) {
        return;
      }
    }
    is_range_only_ = true;
  }

  vector<LogBloom> bloomPossibilities() const {
    // return combination of each of the addresses/topics
    vector<LogBloom> ret;
    // | every address with every topic
    for (auto const& i : addresses_) {
      // 1st case, there are addresses and topics
      //
      // m_addresses = [a0, a1];
      // m_topics = [[t0], [t1a, t1b], [], []];
      //
      // blooms = [
      // a0 | t0, a0 | t1a | t1b,
      // a1 | t0, a1 | t1a | t1b
      // ]
      //
      for (auto const& t : topics_) {
        if (t.empty()) {
          continue;
        }
        auto b = LogBloom().shiftBloom<3>(sha3(i));
        for (auto const& j : t) {
          b = b.shiftBloom<3>(sha3(j));
        }
        ret.push_back(b);
      }
    }

    // 2nd case, there are no topics
    //
    // m_addresses = [a0, a1];
    // m_topics = [[t0], [t1a, t1b], [], []];
    //
    // blooms = [a0, a1];
    //
    if (ret.empty()) {
      for (auto const& i : addresses_) {
        ret.push_back(LogBloom().shiftBloom<3>(sha3(i)));
      }
    }

    // 3rd case, there are no addresses, at least create blooms from topics
    //
    // m_addresses = [];
    // m_topics = [[t0], [t1a, t1b], [], []];
    //
    // blooms = [t0, t1a | t1b];
    //
    if (addresses_.empty()) {
      for (auto const& t : topics_) {
        if (t.size()) {
          LogBloom b;
          for (auto const& j : t) {
            b = b.shiftBloom<3>(sha3(j));
          }
          ret.push_back(b);
        }
      }
    }
    return ret;
  }

  bool matches(LogBloom b) const {
    if (!addresses_.empty()) {
      auto ok = false;
      for (auto const& i : addresses_) {
        if (b.containsBloom<3>(sha3(i))) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return false;
      }
    }
    for (auto const& t : topics_) {
      if (t.empty()) {
        continue;
      }
      auto ok = false;
      for (auto const& i : t) {
        if (b.containsBloom<3>(sha3(i))) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return false;
      }
    }
    return true;
  }

  void match_one(TransactionReceipt const& r, function<void(size_t)> const& cb) const {
    if (!matches(r.bloom())) {
      return;
    }
    for (size_t log_i = 0; log_i < r.logs.size(); ++log_i) {
      auto const& e = r.logs[log_i];
      if (!addresses_.empty() && !addresses_.count(e.address)) {
        continue;
      }
      auto ok = true;
      for (size_t i = 0; i < topics_.size(); ++i) {
        if (!topics_[i].empty() && (e.topics.size() < i || !topics_[i].count(e.topics[i]))) {
          ok = false;
          break;
        }
      }
      if (ok) {
        cb(log_i);
      }
    }
  }

 public:
  bool blk_number_matches(BlockNumber blk_n) const {
    return from_block_ && *from_block_ <= blk_n && to_block_ && blk_n <= *to_block_;
  }

  void match_one(ExtendedTransactionLocation const& trx_loc, TransactionReceipt const& r,
                 function<void(LocalisedLogEntry const&)> const& cb) const {
    if (!blk_number_matches(trx_loc.blk_n)) {
      return;
    }
    auto action = [&](size_t log_i) { cb({r.logs[log_i], trx_loc, log_i}); };
    if (!is_range_only_) {
      match_one(r, action);
      return;
    }
    for (size_t i = 0; i < r.logs.size(); ++i) {
      action(i);
    }
  }

  vector<LocalisedLogEntry> match_all(FinalChain const& final_chain) const {
    vector<LocalisedLogEntry> ret;
    auto action = [&, this](BlockNumber blk_n) {
      ExtendedTransactionLocation trx_loc{{{blk_n, 0}, *final_chain.block_hash(blk_n)}, {}};
      final_chain.transaction_hashes(trx_loc.blk_n)->for_each([&, this](auto const& trx_h) {
        trx_loc.trx_hash = trx_h;
        match_one(trx_loc, *final_chain.transaction_receipt(trx_h), [&](auto const& lle) { ret.push_back(lle); });
        ++trx_loc.index;
      });
    };
    auto from_blk_n = from_block_ ? *from_block_ : final_chain.last_block_number();
    auto to_blk_n = to_block_ ? *to_block_ : (!from_block_ ? from_blk_n : final_chain.last_block_number());
    if (is_range_only_) {
      for (auto blk_n = from_blk_n; blk_n <= to_blk_n; ++blk_n) {
        action(blk_n);
      }
      return ret;
    }
    set<BlockNumber> matchingBlocks;
    for (auto const& bloom : bloomPossibilities()) {
      for (auto blk_n : final_chain.withBlockBloom(bloom, from_blk_n, to_blk_n)) {
        matchingBlocks.insert(blk_n);
      }
    }
    for (auto blk_n : matchingBlocks) {
      action(blk_n);
    }
    return ret;
  }
};

}  // namespace taraxa::net::rpc::eth