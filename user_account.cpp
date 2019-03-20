/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-12 13:31:22
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-28 22:40:41
 */

#include "user_account.hpp"
#include <iostream>
#include <string>

using std::string;
using std::to_string;

namespace taraxa {

UserAccount::UserAccount(name_t address, key_t pk, blk_hash_t genesis,
                         bal_t balance, blk_hash_t frontier, uint64_t height)
    : address_(address),
      pk_(pk),
      genesis_(genesis),
      balance_(balance),
      frontier_(frontier),
      height_(height) {}

UserAccount::UserAccount(string const &json) {
  boost::property_tree::ptree doc = strToJson(json);
  address_ = doc.get<std::string>("address");
  pk_ = doc.get<std::string>("pk");
  genesis_ = doc.get<std::string>("genesis");
  balance_ = doc.get<uint64_t>("balance");
  frontier_ = doc.get<std::string>("frontier");
  height_ = doc.get<uint64_t>("height");
}

std::string UserAccount::getJsonStr() {
  boost::property_tree::ptree doc;

  doc.put("address", address_);
  doc.put("pk", pk_);
  doc.put("genesis", genesis_);
  doc.put("balance", balance_);
  doc.put("frontier", frontier_);
  doc.put("height", height_);

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, doc);
  return ostrm.str();
}

}  // namespace taraxa
