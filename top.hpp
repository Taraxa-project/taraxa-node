/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-19 12:56:25
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-22 14:54:34
 */

#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/LoggingProgramOptions.h"
#include "rpc.hpp"

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
  std::shared_ptr<taraxa::Rpc>& getRpc() { return rpc_; }

 private:
  bool stopped_ = true;
  bool boot_node_ = false;
  std::shared_ptr<std::thread> th_;
  std::shared_ptr<taraxa::FullNode> node_;
  std::shared_ptr<taraxa::Rpc> rpc_;
  std::condition_variable cond_;
  std::mutex mu_;
  boost::asio::io_context context_;
};
