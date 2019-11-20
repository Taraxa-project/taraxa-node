#include <jsonrpccpp/common/exception.h>
#include <libethcore/CommonJS.h>
#include <libethcore/KeyManager.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/JsonHelperTaraxa.h>

#include "Personal.h"

using namespace std;
using namespace dev;
using namespace dev::rpc;
using namespace dev::eth;
using namespace jsonrpc;

Personal::Personal(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

std::string Personal::personal_newAccount(std::string const& _password) {
  return "";
}

string Personal::personal_sendTransaction(Json::Value const& _transaction,
                                          string const& _password) {
  return "";
}

string Personal::personal_signAndSendTransaction(
    Json::Value const& _transaction, string const& _password) {
  return "";
}

bool Personal::personal_unlockAccount(std::string const& _address,
                                      std::string const& _password,
                                      int _duration) {
  return false;
}

Json::Value Personal::personal_listAccounts() { return ""; }
