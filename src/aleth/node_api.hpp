#ifndef TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_
#define TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_

#include <libweb3jsonrpc/Eth.h>

#include <functional>
#include <transaction.hpp>

namespace taraxa::aleth {

std::unique_ptr<dev::rpc::Eth::NodeAPI> NewNodeAPI(uint64_t chain_id, dev::KeyPair key_pair,
                                                   std::function<void(::taraxa::Transaction const&)> send_trx);

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_TARAXA_ALETH_NODE_API_HPP_
