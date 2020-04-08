//
// Created by Qi Gao on 2019-10-24.
//

#include "pbft_sortition_account.hpp"

namespace taraxa {

PbftSortitionAccount::PbftSortitionAccount(std::string const& json) {
  Json::Value doc;
  Json::Reader reader;
  reader.parse(json, doc);

  address = addr_t(doc["address"].asString());
  balance = val_t(doc["balance"].asString());
  last_period_seen = doc["last_period_seen"].asInt64();
  status = static_cast<PbftSortitionAccountStatus>(doc["status"].asInt());
}

std::string PbftSortitionAccount::getJsonStr() const {
  Json::Value json;
  json["address"] = address.toString();
  json["balance"] = balance.str();
  json["last_period_seen"] = Json::Value::Int64(last_period_seen);
  json["status"] = status;
  return json.toStyledString();
}

}  // namespace taraxa
