/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_TARAXA_NET_TARAXACLIENT_H_
#define JSONRPC_CPP_STUB_TARAXA_NET_TARAXACLIENT_H_

#include <jsonrpccpp/client.h>

namespace taraxa {
namespace net {
class TaraxaClient : public jsonrpc::Client {
 public:
  TaraxaClient(jsonrpc::IClientConnector& conn, jsonrpc::clientVersion_t type = jsonrpc::JSONRPC_CLIENT_V2)
      : jsonrpc::Client(conn, type) {}

  std::string taraxa_protocolVersion() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_protocolVersion", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getVersion() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_getVersion", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getDagBlockByHash(const std::string& param1, bool param2) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    p.append(param2);
    Json::Value result = this->CallMethod("taraxa_getDagBlockByHash", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getDagBlockByLevel(const std::string& param1, bool param2) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    p.append(param2);
    Json::Value result = this->CallMethod("taraxa_getDagBlockByLevel", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  std::string taraxa_dagBlockLevel() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_dagBlockLevel", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  std::string taraxa_dagBlockPeriod() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_dagBlockPeriod", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getScheduleBlockByPeriod(const std::string& param1) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    Json::Value result = this->CallMethod("taraxa_getScheduleBlockByPeriod", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getNodeVersions() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_getNodeVersions", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getConfig() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_getConfig", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  Json::Value taraxa_getChainStats() throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod("taraxa_getChainStats", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
  std::string taraxa_pbftBlockHashByPeriod(const std::string& param1) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    Json::Value result = this->CallMethod("taraxa_pbftBlockHashByPeriod", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }

  std::string taraxa_yield(const std::string& param1) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    Json::Value result = this->CallMethod("taraxa_yield", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }

  std::string taraxa_totalSupply(const std::string& param1) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    Json::Value result = this->CallMethod("taraxa_totalSupply", p);
    if (result.isString())
      return result.asString();
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }

  Json::Value taraxa_getPillarBlockData(const std::string& param1, bool param2) throw(jsonrpc::JsonRpcException) {
    Json::Value p;
    p.append(param1);
    p.append(param2);
    Json::Value result = this->CallMethod("taraxa_getPillarBlockData", p);
    if (result.isObject())
      return result;
    else
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
  }
};

}  // namespace net
}  // namespace taraxa
#endif  // JSONRPC_CPP_STUB_TARAXA_NET_TARAXACLIENT_H_
