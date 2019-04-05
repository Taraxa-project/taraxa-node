/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-03 17:03:47
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-12 13:35:32
 */

#ifndef WALLET_HPP
#define WALLET_HPP

#include <iostream>
#include <memory>
#include <string>
#include "util.hpp"

namespace taraxa {

class RocksDb;

struct WalletConfig {
  WalletConfig(std::string const &json_file);
  std::string db_wallet_path;
  key_t wallet_key;
};

// triplet: sk, pk, address
struct WalletUserAccount {
  WalletUserAccount(std::string const &sk, std::string const &pk,
                    std::string const &address);
  WalletUserAccount(std::string const &json);
  key_t sk;
  key_t pk;
  addr_t address;
  std::string getJsonStr();
};

class Wallet : public std::enable_shared_from_this<Wallet> {
 public:
  Wallet(WalletConfig const &conf);
  std::string accountCreate(key_t sk);
  std::string accountQuery(addr_t address);
  std::shared_ptr<Wallet> getShared() { return shared_from_this(); }

 private:
  WalletConfig conf_;
  key_t wallet_key_ = "0";
  std::shared_ptr<RocksDb> db_wallet_sp_;
};

}  // namespace taraxa

#endif
