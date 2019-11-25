#ifndef TARAXA_NODE_TOP_HPP
#define TARAXA_NODE_TOP_HPP

#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Test.h>
#include <atomic>
#include <boost/process.hpp>
#include "full_node.hpp"
#include <libdevcore/Log.h>
#include <libdevcore/LoggingProgramOptions.h>
#include "net/Net.h"
#include "net/Taraxa.h"
#include "net/Test.h"
#include "net/WSServer.h"

struct Top {
  using Rpc = ModularServer<taraxa::net::TestFace, taraxa::net::TaraxaFace,
                            taraxa::net::NetFace>;

 private:
  boost::asio::io_context rpc_io_context_;
  taraxa::FullNode::container node_;
  std::thread rpc_thread_;

 public:
  Top(int argc, const char* argv[]);
  ~Top();

  // may return null which means there was an error in constructor
  decltype(node_)::shared_t getNode() { return node_; }
  void join();
};

#endif
