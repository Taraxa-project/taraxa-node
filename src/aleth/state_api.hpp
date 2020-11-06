#ifndef TARAXA_NODE_ALETH_STATE_API_HPP_
#define TARAXA_NODE_ALETH_STATE_API_HPP_

#include <libweb3jsonrpc/Eth.h>

#include "../final_chain.hpp"

namespace taraxa::aleth {

std::unique_ptr<dev::rpc::Eth::StateAPI> NewStateAPI(std::shared_ptr<FinalChain> final_chain);

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_ALETH_STATE_API_HPP_
