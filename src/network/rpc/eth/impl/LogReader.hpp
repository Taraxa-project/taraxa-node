#pragma once

#include "data.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::net::rpc::eth {

struct LogReader {
  shared_ptr<FinalChain> final_chain;

  static bool is_range_only(LogFilter const& f) {
    if (!f.addresses.empty()) {
      return false;
    }
    for (auto const& t : f.topics) {
      if (!t.empty()) {
        return false;
      }
    }
    return true;
  }

  /// @returns bloom possibilities for all addresses and topics
  static std::vector<LogBloom> bloomPossibilities(LogFilter const& f) {
    // return combination of each of the addresses/topics
    vector<LogBloom> ret;
    // | every address with every topic
    for (auto const& i : f.addresses) {
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
      for (auto const& t : f.topics) {
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
      for (auto const& i : f.addresses) {
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
    if (f.addresses.empty()) {
      for (auto const& t : f.topics) {
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

  // TODO bloom const&
  static bool matches(LogFilter const& f, LogBloom b) {
    if (!f.addresses.empty()) {
      auto ok = false;
      for (auto const& i : f.addresses) {
        if (b.containsBloom<3>(sha3(i))) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return false;
      }
    }
    for (auto const& t : f.topics) {
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

  static vector<size_t> matches(LogFilter const& f, TransactionReceipt const& r) {
    vector<size_t> ret;
    if (!matches(f, r.bloom())) {
      return ret;
    }
    for (size_t log_i = 0; log_i < r.logs.size(); ++log_i) {
      auto const& e = r.logs[log_i];
      if (!f.addresses.empty() && !f.addresses.count(e.address)) {
        continue;
      }
      auto ok = true;
      for (size_t i = 0; i < f.topics.size(); ++i) {
        if (!f.topics[i].empty() && (e.topics.size() < i || !f.topics[i].count(e.topics[i]))) {
          ok = false;
          break;
        }
      }
      if (ok) {
        ret.push_back(log_i);
      }
    }
    return ret;
  }

  vector<LocalisedLogEntry> logs(LogFilter const& f) const {
    auto range_only = is_range_only(f);
    vector<LocalisedLogEntry> ret;
    auto action = [&](BlockNumber blk_n) {
      TransactionLocationWithBlockHash trx_loc{blk_n, 0, *final_chain->block_hash(blk_n)};
      final_chain->transaction_hashes(trx_loc.blk_n)->for_each([&](auto const& trx_h) {
        auto r = *final_chain->transaction_receipt(trx_h);
        auto action = [&](size_t log_i) {
          ret.push_back({r.logs[log_i], {trx_loc, trx_h}, log_i});
          //
        };
        if (range_only) {
          for (size_t i = 0; i < r.logs.size(); ++i) {
            action(i);
          }
        } else {
          for (auto i : matches(f, r)) {
            action(i);
          }
        }
        ++trx_loc.index;
      });
    };
    if (range_only) {
      for (auto blk_n = f.from_block; blk_n <= f.to_block; ++blk_n) {
        action(blk_n);
      }
      return ret;
    }
    set<BlockNumber> matchingBlocks;
    for (auto const& bloom : bloomPossibilities(f)) {
      for (auto blk_n : final_chain->withBlockBloom(bloom, f.from_block, f.to_block)) {
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