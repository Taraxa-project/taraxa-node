#ifndef TARAXA_NODE_TOP_HPP
#define TARAXA_NODE_TOP_HPP

#include <libweb3jsonrpc/IpcServer.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Test.h>
#include <atomic>
#include <boost/process.hpp>
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/LoggingProgramOptions.h"
#include "libweb3jsonrpc/Net.h"
#include "libweb3jsonrpc/Taraxa.h"
#include "libweb3jsonrpc/WSServer.h"

struct Top {
  using Rpc = ModularServer<dev::rpc::TestFace, dev::rpc::TaraxaFace,
                            dev::rpc::NetFace>;

  Top(int argc, const char* argv[]);
  ~Top();

  void join();
  auto getNode() { return node_; }

 private:
  boost::asio::io_context rpc_io_context_;
  std::shared_ptr<taraxa::FullNode> node_;
  std::thread rpc_thread_;
};

#endif
