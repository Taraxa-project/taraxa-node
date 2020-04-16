//
// Created by Qi Gao on 2019-10-24.
//

#include "pbft_sortition_account.hpp"

namespace taraxa {
using boost::property_tree::ptree;

PbftSortitionAccount::PbftSortitionAccount(std::string const& json) {
  ptree doc = strToJson(json);

  address = addr_t(doc.get<std::string>("address"));
  balance = doc.get<val_t>("balance");
  last_period_seen = doc.get<int64_t>("last_period_seen");
  status = static_cast<PbftSortitionAccountStatus>(doc.get<int>("status"));
}

std::string PbftSortitionAccount::getJsonStr() const {
  ptree tree;
  tree.put("address", address.toString());
  tree.put("balance", balance);
  tree.put("last_period_seen", last_period_seen);
  tree.put("status", status);

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

}  // namespace taraxa
