#pragma once

#include <libweb3jsonrpc/Eth.h>

#include <functional>
#include <transaction_manager/transaction.hpp>

namespace taraxa::aleth {

std::unique_ptr<dev::rpc::Eth::NodeAPI> NewNodeAPI(uint64_t chain_id, dev::KeyPair key_pair,
                                                   std::function<void(::taraxa::Transaction const&)> send_trx);

}  // namespace taraxa::aleth
