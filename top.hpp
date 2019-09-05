#ifndef TARAXA_NODE_TOP_HPP
#define TARAXA_NODE_TOP_HPP
#include <libweb3jsonrpc/IpcServer.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Test.h>
#include <boost/process.hpp>
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/LoggingProgramOptions.h"
#include "libweb3jsonrpc/WSServer.h"

class Top {
 public:
  Top(int argc, const char* argv[]);
  Top(std::string const& command);

  ~Top();
  void start(int argc, const char* argv[]);
  void start();
  void run();
  // stop() won't stop rpc
  void stop();
  void reset();
  // use kill to exit program
  void kill();
  bool isActive() { return !stopped_; }
  std::shared_ptr<taraxa::FullNode>& getNode() { return node_; }
  std::shared_ptr<ModularServer<>>& getRpc() { return rpc_; }
  void startRpc();

 private:
  bool stopped_ = true;
  bool boot_node_ = false;
  std::queue<boost::exception_ptr> exceptions_;
  std::shared_ptr<std::thread> th_;
  std::shared_ptr<taraxa::FullNode> node_;
  std::shared_ptr<ModularServer<>> rpc_;
  std::shared_ptr<taraxa::WSServer> ws_listener_;
  std::condition_variable cond_;
  std::mutex mu_;
  boost::asio::io_context context_;
  std::shared_ptr<taraxa::FullNodeConfig> conf_;
};
#endif
