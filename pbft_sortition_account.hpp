//
// Created by Qi Gao on 2019-10-24.
//

#ifndef TARAXA_NODE_PBFT_SORTITION_ACCOUNT_H
#define TARAXA_NODE_PBFT_SORTITION_ACCOUNT_H

#include "util.hpp"

namespace taraxa {

enum PbftSortitionAccountStatus {
  new_change = 0,
  remove,
  updated,
};

class PbftSortitionAccount {
 public:
  PbftSortitionAccount() = default;
  PbftSortitionAccount(addr_t account_address, val_t account_balance,
                       int64_t pbft_period,
                       PbftSortitionAccountStatus account_status)
      : address(account_address),
        balance(account_balance),
        last_period_seen(pbft_period),
        status(account_status) {}
  PbftSortitionAccount(std::string const& json);
  ~PbftSortitionAccount() {}

  std::string getJsonStr() const;

  addr_t address;
  val_t balance;
  int64_t last_period_seen;  // last PBFT period seen sending transactions, -1
                             // means never seen sending trxs
  PbftSortitionAccountStatus status;
};

}  // namespace taraxa

#endif  // TARAXA_NODE_PBFT_SORTITION_ACCOUNT_H
