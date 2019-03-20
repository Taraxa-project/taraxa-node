/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-05 16:25:14
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 17:00:15
 */

#include "grpc_util.hpp"
namespace taraxa {

void setProtoTransaction(Transaction const& t,
                         ::taraxa_grpc::ProtoTransaction* ret) {
  ret->set_hash(t.getHash().toString());
  ret->set_type(asInteger(t.getType()));
  ret->set_nonce(t.getNonce().toString());
  ret->set_value(t.getValue().toString());
  ret->set_gas_price(t.getGasPrice().toString());
  ret->set_gas(t.getGas().toString());
  ret->set_receiver(t.getReceiver().toString());
  ret->set_sig(t.getSig().toString());
  ret->set_data(bytes2str(t.getData()));
}

}  // namespace taraxa