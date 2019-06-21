#include "AdminTaraxa.h"
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>
#include "JsonHelper.h"
#include "SessionManager.h"
//#include <libethashseal/Ethash.h>
#include <libethcore/KeyManager.h>
//#include <libethereum/Client.h>
//#include <libethereum/Executive.h>
using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;

AdminTaraxa::AdminTaraxa(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

bool AdminTaraxa::admin_taraxa_setMining(bool _on, string const& _session) {
  return true;
}

Json::Value AdminTaraxa::admin_taraxa_blockQueueStatus(string const& _session) {
  return "";
}

bool AdminTaraxa::admin_taraxa_setAskPrice(string const& _wei,
                                           string const& _session) {
  return false;
}

bool AdminTaraxa::admin_taraxa_setBidPrice(string const& _wei,
                                           string const& _session) {
  return false;
}

Json::Value AdminTaraxa::admin_taraxa_findBlock(string const& _blockHash,
                                                string const& _session) {
  return "";
}

string AdminTaraxa::admin_taraxa_blockQueueFirstUnknown(
    string const& _session) {
  return "";
}

bool AdminTaraxa::admin_taraxa_blockQueueRetryUnknown(string const& _session) {
  return false;
}

Json::Value AdminTaraxa::admin_taraxa_allAccounts(string const& _session) {
  return "";
}

Json::Value AdminTaraxa::admin_taraxa_newAccount(Json::Value const& _info,
                                                 string const& _session) {
  return "";
}

bool AdminTaraxa::admin_taraxa_setMiningBenefactor(string const& _uuidOrAddress,
                                                   string const& _session) {
  return false;
}

Json::Value AdminTaraxa::admin_taraxa_inspect(string const& _address,
                                              string const& _session) {
  return "";
}

Json::Value AdminTaraxa::admin_taraxa_reprocess(
    string const& _blockNumberOrHash, string const& _session) {
  return "";
}

Json::Value AdminTaraxa::admin_taraxa_vmTrace(string const& _blockNumberOrHash,
                                              int _txIndex,
                                              string const& _session) {
  return "";
}

Json::Value AdminTaraxa::admin_taraxa_getReceiptByHashAndIndex(
    string const& _blockNumberOrHash, int _txIndex, string const& _session) {
  return "";
}

bool AdminTaraxa::miner_start(int) { return false; }

bool AdminTaraxa::miner_stop() { return false; }

bool AdminTaraxa::miner_setEtherbase(string const& _uuidOrAddress) {
  return false;
}

bool AdminTaraxa::miner_setExtra(string const& _extraData) { return false; }

bool AdminTaraxa::miner_setGasPrice(string const& _gasPrice) { return false; }

string AdminTaraxa::miner_hashrate() { return ""; }
