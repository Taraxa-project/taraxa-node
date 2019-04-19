/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-05 16:23:27
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 16:58:36
 */

#ifndef GRPC_UTIL_HPP
#define GRPC_UTIL_HPP
#include "proto/taraxa_grpc.grpc.pb.h"
#include "transaction.hpp"
#include "libethcore/Common.h"
namespace taraxa {

void setProtoTransaction(Transaction const& t,
                         ::taraxa_grpc::ProtoTransaction* ret);
void runGrpcServer();
}  // namespace taraxa

#endif
