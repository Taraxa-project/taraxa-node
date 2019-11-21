#include "Debug.h"

using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;

Debug::Debug(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

Json::Value Debug::debug_traceTransaction(string const& _txHash,
                                          Json::Value const& _json) {
  return "";
}

Json::Value Debug::debug_traceBlock(string const& _blockRLP,
                                    Json::Value const& _json) {
  return "";
}

Json::Value Debug::debug_traceBlockByHash(string const& _blockHash,
                                          Json::Value const& _json) {
  return "";
}

Json::Value Debug::debug_traceBlockByNumber(int _blockNumber,
                                            Json::Value const& _json) {
  return "";
}

Json::Value Debug::debug_accountRangeAt(string const& _blockHashOrNumber,
                                        int _txIndex,
                                        string const& _addressHash,
                                        int _maxResults) {
  return "";
}

Json::Value Debug::debug_storageRangeAt(string const& _blockHashOrNumber,
                                        int _txIndex, string const& _address,
                                        string const& _begin, int _maxResults) {
  return "";
}

std::string Debug::debug_preimage(std::string const& _hashedKey) { return ""; }

Json::Value Debug::debug_traceCall(Json::Value const& _call,
                                   std::string const& _blockNumber,
                                   Json::Value const& _options) {
  return "";
}
