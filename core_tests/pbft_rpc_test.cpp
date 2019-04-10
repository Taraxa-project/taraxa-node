/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-09
 * @Last Modified by:
 * @Last Modified time:
 */
#include "full_node.hpp"
#include "libdevcore/Log.h"
#include "network.hpp"
#include "rpc.hpp"

#include <boost/thread.hpp>
#include <gtest/gtest.h>

namespace taraxa {

TEST(PbftRpc, pbft_should_speak_test) {
  boost::asio::io_context context1;

  auto node1(std::make_shared<taraxa::FullNode>(
      context1,
      std::string("./core_tests/conf_full_node1.json"),
      std::string("./core_tests/conf_network1.json")));
  auto rpc(std::make_shared<taraxa::Rpc>(
      context1, "./core_tests/conf_rpc1.json", node1->getShared()));
  rpc->start();
  node1->setDebug(true);
  node1->start();

  std::unique_ptr<boost::asio::io_context::work> work(
      new boost::asio::io_context::work(context1));

  boost::thread t([&context1]() {
    context1.run();
  });

  try {
    system("./core_tests/curl_pbft_should_speak.sh");
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  taraxa::thisThreadSleepForMilliSeconds(500);

  work.reset();
  node1->stop();
  rpc->stop();
  t.join();
}

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityError;
  logOptions.includeChannels.push_back("PBFT");
  logOptions.includeChannels.push_back("RPC");

  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}