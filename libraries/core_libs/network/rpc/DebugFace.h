/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_TARAXA_NET_DEBUGFACE_H_
#define JSONRPC_CPP_STUB_TARAXA_NET_DEBUGFACE_H_

#include <libweb3jsonrpc/ModularServer.h>

namespace taraxa {
    namespace net {
        class DebugFace : public ServerInterface<DebugFace>
        {
            public:
                DebugFace()
                {
                    this->bindAndAddMethod(jsonrpc::Procedure("debug_traceTransaction", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param1",jsonrpc::JSON_STRING,"param2",jsonrpc::JSON_OBJECT, NULL), &taraxa::net::DebugFace::debug_traceTransactionI);
                    this->bindAndAddMethod(jsonrpc::Procedure("debug_traceCall", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param1",jsonrpc::JSON_OBJECT,"param2",jsonrpc::JSON_STRING,"param3",jsonrpc::JSON_OBJECT, NULL), &taraxa::net::DebugFace::debug_traceCallI);
                }

                inline virtual void debug_traceTransactionI(const Json::Value &request, Json::Value &response)
                {
                    response = this->debug_traceTransaction(request[0u].asString(), request[1u]);
                }
                inline virtual void debug_traceCallI(const Json::Value &request, Json::Value &response)
                {
                    response = this->debug_traceCall(request[0u], request[1u].asString(), request[2u]);
                }
                virtual Json::Value debug_traceTransaction(const std::string& param1, const Json::Value& param2) = 0;
                virtual Json::Value debug_traceCall(const Json::Value& param1, const std::string& param2, const Json::Value& param3) = 0;
        };

    }
}
#endif //JSONRPC_CPP_STUB_TARAXA_NET_DEBUGFACE_H_
