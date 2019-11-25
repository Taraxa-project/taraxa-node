#ifndef TARAXA_NODE_NET_JSON_HELPER_H_
#define TARAXA_NODE_NET_JSON_HELPER_H_

#include <json/json.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/Common.h>
#include <libethereum/TransactionReceipt.h>
#include <libp2p/Common.h>
#include <optional>
#include "../transaction.hpp"

namespace taraxa::net {

Json::Value toJson(
    std::map<dev::h256, std::pair<dev::u256, dev::u256>> const& _storage);
Json::Value toJson(std::unordered_map<dev::u256, dev::u256> const& _storage);
Json::Value toJson(dev::Address const& _address);
Json::Value toJson(
    taraxa::Transaction const&,
    std::optional<taraxa::TransactionPosition> const& = std::nullopt);
// fixme: DEPRECATED
Json::Value toJson(std::shared_ptr<::taraxa::DagBlock> block,
                   uint64_t block_height);

inline auto const JSON_NULL = Json::Value();

Json::Value toJson(dev::p2p::PeerSessionInfo const& _p);

dev::h256 h256fromHex(std::string const& _s);

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

}  // namespace taraxa::net

#endif