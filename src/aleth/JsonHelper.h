#pragma once

#include <json/json.h>
#include <libdevcore/CommonJS.h>

#include <optional>
#include <utility>

#include "BlockDetails.h"
#include "BlockHeader.h"
#include "Common.h"
#include "LogFilter.h"
#include "Transaction.h"
#include "types.hpp"

namespace taraxa::aleth {

Json::Value toJson(std::map<dev::h256, std::pair<dev::u256, dev::u256>> const& _storage);
Json::Value toJson(std::unordered_map<dev::u256, dev::u256> const& _storage);

template <typename T>
Json::Value toJson(T const& t) {
  return dev::toJS(t);
}

template <typename T>
Json::Value toJson(std::optional<T> const& t) {
  return t ? dev::toJS(*t) : Json::Value(Json::nullValue);
}

using UncleHashes = dev::h256s;
using TransactionHashes = dev::h256s;

Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi);
Json::Value toJson(::taraxa::aleth::Transaction const& _t, std::pair<dev::h256, unsigned> _location,
                   ::taraxa::aleth::BlockNumber _blockNumber);
Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi, ::taraxa::aleth::BlockDetails const& _bd,
                   UncleHashes const& _us, std::vector<::taraxa::aleth::Transaction> const& _ts);
Json::Value toJson(::taraxa::aleth::BlockHeader const& _bi, ::taraxa::aleth::BlockDetails const& _bd,
                   UncleHashes const& _us, TransactionHashes const& _ts);
Json::Value toJson(::taraxa::aleth::TransactionSkeleton const& _t);
Json::Value toJson(::taraxa::aleth::Transaction const& _t);
Json::Value toJson(::taraxa::aleth::Transaction const& _t, dev::bytes const& _rlp);
Json::Value toJson(::taraxa::aleth::LocalisedTransaction const& _t);
Json::Value toJson(::taraxa::aleth::TransactionReceipt const& _t);
Json::Value toJson(::taraxa::aleth::LocalisedTransactionReceipt const& _t);
Json::Value toJson(::taraxa::aleth::LocalisedLogEntry const& _e);
Json::Value toJson(::taraxa::aleth::LogEntry const& _e);
::taraxa::aleth::TransactionSkeleton toTransactionSkeleton(Json::Value const& _json);

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

}  // namespace taraxa::aleth
