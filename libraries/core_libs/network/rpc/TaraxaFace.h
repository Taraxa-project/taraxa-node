/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_TARAXA_NET_TARAXAFACE_H_
#define JSONRPC_CPP_STUB_TARAXA_NET_TARAXAFACE_H_

#include <libweb3jsonrpc/ModularServer.h>

namespace taraxa {
namespace net {
class TaraxaFace : public ServerInterface<TaraxaFace> {
 public:
  static constexpr int JSON_ANY = 0;

  TaraxaFace() {
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_protocolVersion", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL),
        &taraxa::net::TaraxaFace::taraxa_protocolVersionI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_getVersion", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TaraxaFace::taraxa_getVersionI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_getDagBlockByHash", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param1",
                           jsonrpc::JSON_STRING, "param2", jsonrpc::JSON_BOOLEAN, NULL),
        &taraxa::net::TaraxaFace::taraxa_getDagBlockByHashI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_getDagBlockByLevel", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param1",
                           jsonrpc::JSON_STRING, "param2", jsonrpc::JSON_BOOLEAN, NULL),
        &taraxa::net::TaraxaFace::taraxa_getDagBlockByLevelI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_dagBlockLevel", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL),
        &taraxa::net::TaraxaFace::taraxa_dagBlockLevelI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_dagBlockPeriod", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL),
        &taraxa::net::TaraxaFace::taraxa_dagBlockPeriodI);
    this->bindAndAddMethod(jsonrpc::Procedure("taraxa_getScheduleBlockByPeriod", jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_STRING, NULL),
                           &taraxa::net::TaraxaFace::taraxa_getScheduleBlockByPeriodI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_getConfig", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TaraxaFace::taraxa_getConfigI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_getChainStats", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL),
        &taraxa::net::TaraxaFace::taraxa_getChainStatsI);
    this->bindAndAddMethod(jsonrpc::Procedure("taraxa_pbftBlockHashByPeriod", jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_STRING, "param1", jsonrpc::JSON_STRING, NULL),
                           &taraxa::net::TaraxaFace::taraxa_pbftBlockHashByPeriodI);
    this->bindAndAddMethod(
        jsonrpc::Procedure("taraxa_yield", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param1", JSON_ANY, NULL),
        &taraxa::net::TaraxaFace::taraxa_yieldI);
    this->bindAndAddMethod(jsonrpc::Procedure("taraxa_totalSupply", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                                              "param1", JSON_ANY, NULL),
                           &taraxa::net::TaraxaFace::taraxa_totalSupplyI);
    this->bindAndAddMethod(jsonrpc::Procedure("taraxa_getPillarBlock", jsonrpc::PARAMS_BY_POSITION,
                                              jsonrpc::JSON_OBJECT, "param1", JSON_ANY, NULL),
                           &taraxa::net::TaraxaFace::taraxa_getPillarBlockI);
  }

  inline virtual void taraxa_protocolVersionI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_protocolVersion();
  }
  inline virtual void taraxa_getVersionI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_getVersion();
  }
  inline virtual void taraxa_getDagBlockByHashI(const Json::Value &request, Json::Value &response) {
    response = this->taraxa_getDagBlockByHash(request[0u].asString(), request[1u].asBool());
  }
  inline virtual void taraxa_getDagBlockByLevelI(const Json::Value &request, Json::Value &response) {
    response = this->taraxa_getDagBlockByLevel(request[0u].asString(), request[1u].asBool());
  }
  inline virtual void taraxa_dagBlockLevelI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_dagBlockLevel();
  }
  inline virtual void taraxa_dagBlockPeriodI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_dagBlockPeriod();
  }
  inline virtual void taraxa_getScheduleBlockByPeriodI(const Json::Value &request, Json::Value &response) {
    response = this->taraxa_getScheduleBlockByPeriod(request[0u].asString());
  }
  inline virtual void taraxa_getConfigI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_getConfig();
  }
  inline virtual void taraxa_getChainStatsI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_getChainStats();
  }
  inline virtual void taraxa_pbftBlockHashByPeriodI(const Json::Value &request, Json::Value &response) {
    response = this->taraxa_pbftBlockHashByPeriod(request[0u].asString());
  }
  inline virtual void taraxa_yieldI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_yield(request[0u].asString());
  }
  inline virtual void taraxa_totalSupplyI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_totalSupply(request[0u].asString());
  }
  inline virtual void taraxa_getPillarBlockI(const Json::Value &request, Json::Value &response) {
    (void)request;
    response = this->taraxa_getPillarBlock(request[0u].asString());
  }

  virtual std::string taraxa_protocolVersion() = 0;
  virtual Json::Value taraxa_getVersion() = 0;
  virtual Json::Value taraxa_getDagBlockByHash(const std::string &param1, bool param2) = 0;
  virtual Json::Value taraxa_getDagBlockByLevel(const std::string &param1, bool param2) = 0;
  virtual std::string taraxa_dagBlockLevel() = 0;
  virtual std::string taraxa_dagBlockPeriod() = 0;
  virtual Json::Value taraxa_getScheduleBlockByPeriod(const std::string &param1) = 0;
  virtual Json::Value taraxa_getConfig() = 0;
  virtual Json::Value taraxa_getChainStats() = 0;
  virtual std::string taraxa_pbftBlockHashByPeriod(const std::string &param1) = 0;
  virtual std::string taraxa_yield(const std::string &param1) = 0;
  virtual std::string taraxa_totalSupply(const std::string &param1) = 0;
  virtual Json::Value taraxa_getPillarBlock(const std::string &param1) = 0;
};

}  // namespace net
}  // namespace taraxa
#endif  // JSONRPC_CPP_STUB_TARAXA_NET_TARAXAFACE_H_
