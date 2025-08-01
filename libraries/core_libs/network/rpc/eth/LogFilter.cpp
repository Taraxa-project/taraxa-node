#include "LogFilter.hpp"

#include <jsonrpccpp/common/exception.h>

#include "Eth.h"

namespace taraxa::net::rpc::eth {

LogFilter::LogFilter(EthBlockNumber from_block, std::optional<EthBlockNumber> to_block, AddressSet addresses,
                     LogFilter::Topics topics)
    : from_block_(from_block), to_block_(to_block), addresses_(std::move(addresses)), topics_(std::move(topics)) {
  if (!addresses_.empty()) {
    return;
  }
  is_range_only_ =
      addresses_.empty() && std::all_of(topics_.cbegin(), topics_.cend(), [](const auto& t) { return t.empty(); });
}

std::vector<LogBloom> LogFilter::bloomPossibilities() const {
  // We need to match every element that is added to the bloom filter. And to have OR logic h ere we need to construct
  // multiple blooms | every address with every topic option addresses_ = [a0, a1]; topics_ = [[t0], [t1a, t1b], [],
  // []]; blooms = [
  //   a0 | t0 | t1a,
  //   a0 | t0 | t1b,
  //   a1 | t0 | t1a,
  //   a1 | t0 | t1b
  // ]
  // init with addresses
  std::vector<LogBloom> ret;
  ret.reserve(std::accumulate(topics_.begin(), topics_.end(), addresses_.size(),
                              [](size_t res, const auto& t) { return res + t.size(); }));
  std::transform(addresses_.begin(), addresses_.end(), std::back_inserter(ret),
                 [](const auto& a) { return LogBloom().shiftBloom<3>(sha3(a)); });

  if (addresses_.empty()) {
    ret.push_back(LogBloom());
  }

  for (const auto& topic : topics_) {
    // blooms won't change, continue
    if (topic.empty()) {
      continue;
    }
    // move blooms from previous iteration
    std::vector<LogBloom> local(std::move(ret));
    ret.clear();
    for (const auto& bloom : local) {
      std::transform(topic.begin(), topic.end(), std::back_inserter(ret), [&bloom](const auto& o) {
        // copy to not modify original bloom for future use and move it below
        LogBloom b = bloom;
        return std::move(b.shiftBloom<3>(sha3(o)));
      });
    }
  }
  return ret;
}

bool LogFilter::matches(LogBloom b) const {
  if (!addresses_.empty()) {
    if (std::none_of(addresses_.cbegin(), addresses_.cend(), [&b](const auto& i) {
          if (b.containsBloom<3>(sha3(i))) {
            return true;
          }
          return false;
        })) {
      return false;
    }
  }
  for (const auto& t : topics_) {
    if (t.empty()) {
      continue;
    }

    if (std::none_of(t.cbegin(), t.cend(), [&b](const auto& i) {
          if (b.containsBloom<3>(sha3(i))) {
            return true;
          }
          return false;
        })) {
      return false;
    }
  }
  return true;
}

void LogFilter::match_one(const TransactionReceipt& r, const std::function<void(size_t)>& cb) const {
  if (!matches(r.bloom())) {
    return;
  }
  for (size_t log_i = 0; log_i < r.logs.size(); ++log_i) {
    const auto& e = r.logs[log_i];
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

bool LogFilter::blk_number_matches(EthBlockNumber blk_n) const {
  return from_block_ <= blk_n && (!to_block_ || blk_n <= *to_block_);
}

void LogFilter::match_one(const ExtendedTransactionLocation& trx_loc, const TransactionReceipt& r,
                          const std::function<void(const LocalisedLogEntry&)>& cb) const {
  if (!blk_number_matches(trx_loc.period)) {
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

std::vector<LocalisedLogEntry> LogFilter::match_all(const final_chain::FinalChain& final_chain) const {
  std::vector<LocalisedLogEntry> ret;

  auto action = [&, this](EthBlockNumber blk_n) {
    ExtendedTransactionLocation trx_loc{{{blk_n}, *final_chain.blockHash(blk_n)}};
    auto hashes = final_chain.transactionHashes(trx_loc.period);
    auto block_receipts = final_chain.blockReceipts(blk_n);

    if (block_receipts && block_receipts->size() == hashes->size()) {
      for (uint32_t i = 0; i < block_receipts->size(); i++) {
        trx_loc.trx_hash = (*hashes)[i];
        match_one(trx_loc, (*block_receipts)[i], [&](const auto& lle) { ret.push_back(lle); });
        ++trx_loc.position;
      }
    } else {
      for (const auto& hash : *hashes) {
        trx_loc.trx_hash = hash;
        match_one(trx_loc, *final_chain.transactionReceipt(trx_loc.period, trx_loc.position, hash),
                  [&](const auto& lle) { ret.push_back(lle); });
        ++trx_loc.position;
      }
    }
  };
  // to_block can't be greater than the last executed block number
  const auto last_block_number = final_chain.lastBlockNumber();
  auto to_blk_n = to_block_ ? *to_block_ : last_block_number;
  if (to_blk_n > last_block_number) {
    to_blk_n = last_block_number;
  }

  if (is_range_only_) {
    for (auto blk_n = from_block_; blk_n <= to_blk_n; ++blk_n) {
      action(blk_n);
    }
    return ret;
  }
  std::set<EthBlockNumber> matchingBlocks;
  for (const auto& bloom : bloomPossibilities()) {
    for (auto blk_n : final_chain.withBlockBloom(bloom, from_block_, to_blk_n)) {
      matchingBlocks.insert(blk_n);
    }
  }
  for (auto blk_n : matchingBlocks) {
    action(blk_n);
  }
  return ret;
}

AddressSet parse_addresses(const Json::Value& json) {
  AddressSet addresses;
  if (const auto& address = json["address"]; !address.empty()) {
    if (address.isArray()) {
      for (const auto& obj : address) {
        addresses.insert(toAddress(obj.asString()));
      }
    } else {
      addresses.insert(toAddress(address.asString()));
    }
  }
  return addresses;
}
LogFilter::Topics parse_topics(const Json::Value& json) {
  LogFilter::Topics topics;
  if (const auto& topics_json = json["topics"]; !topics_json.empty()) {
    if (topics_json.size() > 4) {
      BOOST_THROW_EXCEPTION(jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS,
                                                      "only up to four topic slots may be specified"));
    }
    for (uint32_t i = 0; i < topics_json.size(); i++) {
      const auto& topic_json = topics_json[i];
      if (topic_json.isArray()) {
        for (const auto& t : topic_json) {
          if (!t.isNull()) {
            topics[i].insert(dev::jsToFixed<32>(t.asString()));
          }
        }
      } else if (!topic_json.isNull()) {
        topics[i].insert(dev::jsToFixed<32>(topic_json.asString()));
      }
    }
  }
  return topics;
}

}  // namespace taraxa::net::rpc::eth
