#pragma once

#include <libweb3jsonrpc/Eth.h>

#include "chain/final_chain.hpp"

namespace taraxa::aleth {

std::unique_ptr<dev::rpc::Eth::StateAPI> NewStateAPI(std::shared_ptr<FinalChain> final_chain);

}  // namespace taraxa::aleth
