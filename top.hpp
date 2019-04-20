/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-19 12:56:25
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-19 18:12:29
 */

#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "libdevcore/LoggingProgramOptions.h"
#include "rpc.hpp"

class Top {
 public:
  Top(int argc, const char* argv[]);
  Top(std::string const& command);

  // Top(std::vector<std::string> strs);
  ~Top();
  void start(int argc, const char* argv[]);
  void run();
  void stop();
  bool isActive() { return !stopped_; }

 private:
  bool stopped_ = true;
  std::shared_ptr<std::thread> th_;
  std::shared_ptr<taraxa::FullNode> node_;
  std::shared_ptr<taraxa::Rpc> rpc_;
  std::condition_variable cond_;
  std::mutex mu_;
  boost::asio::io_context context_;
};
