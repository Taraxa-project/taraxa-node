#ifndef TARAXA_NODE_ALETH_STATE_API_HPP_
#define TARAXA_NODE_ALETH_STATE_API_HPP_

#include "../final_chain.hpp"
#include "net/Eth.h"

namespace taraxa::aleth {

std::unique_ptr<net::Eth::StateAPI> NewStateAPI(std::shared_ptr<FinalChain> final_chain);

}  // namespace taraxa::aleth

#endif  // TARAXA_NODE_ALETH_STATE_API_HPP_
