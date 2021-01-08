#pragma once

#include <json/json.h>
#include <libdevcore/CommonJS.h>

#include <optional>
#include <utility>

#include "BlockDetails.h"
#include "BlockHeader.h"
#include "Common.h"
#include "LocalizedTransaction.h"
#include "LogFilter.h"
#include "types.hpp"

namespace taraxa::aleth {

Json::Value toJson(std::map<h256, std::pair<u256, u256>> const& _storage);
Json::Value toJson(std::unordered_map<u256, u256> const& _storage);

template <typename T>
Json::Value toJson(T const& t) {
  return dev::toJS(t);
}

using UncleHashes = h256s;
using TransactionHashes = h256s;

Json::Value toJson(BlockHeader const& _bi);
Json::Value toJson(Transaction const& _t, std::pair<h256, unsigned> _location, BlockNumber _blockNumber);
Json::Value toJson(BlockHeader const& _bi, BlockDetails const& _bd, UncleHashes const& _us,
                   std::vector<Transaction> const& _ts);
Json::Value toJson(BlockHeader const& _bi, BlockDetails const& _bd, UncleHashes const& _us,
                   TransactionHashes const& _ts);
Json::Value toJson(TransactionSkeleton const& _t);
Json::Value toJson(Transaction const& _t);
Json::Value toJson(Transaction const& _t, bytes const& _rlp);
Json::Value toJson(LocalisedTransaction const& _t);
Json::Value toJson(TransactionReceipt const& _t);
Json::Value toJson(LocalisedTransactionReceipt const& _t);
Json::Value toJson(LocalisedLogEntry const& _e);
Json::Value toJson(LogEntry const& _e);
TransactionSkeleton toTransactionSkeleton(Json::Value const& _json);

template <class T>
Json::Value toJson(std::vector<T> const& _es) {
  Json::Value res(Json::arrayValue);
  for (auto const& e : _es) res.append(toJson(e));
  return res;
}

template <class T>
Json::Value toJson(std::unordered_set<T> const& _es) {
  Json::Value res(Json::arrayValue);
  for (auto const& e : _es) res.append(toJson(e));
  return res;
}

template <class T>
Json::Value toJson(std::set<T> const& _es) {
  Json::Value res(Json::arrayValue);
  for (auto const& e : _es) res.append(toJson(e));
  return res;
}

template <typename T>
Json::Value toJson(std::optional<T> const& t) {
  return t ? toJson(*t) : Json::Value(Json::nullValue);
}

}  // namespace taraxa::aleth
