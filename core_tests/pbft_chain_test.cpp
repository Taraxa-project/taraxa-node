/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-08 15:59:01
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 18:57:12
 */

#include "pbft_chain.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <boost/thread.hpp>
#include <iostream>
#include <vector>
#include "libdevcore/Log.h"
#include "pbft_manager.hpp"
#include "util.hpp"

namespace taraxa {
TEST(TrxSchedule, serialize_deserialize) {
  vec_blk_t blks{blk_hash_t(123), blk_hash_t(456), blk_hash_t(32443)};
  std::vector<std::vector<uint>> modes{
      {0, 1, 2, 0, 1, 2}, {1, 1, 1, 1, 1}, {0, 0, 0}};
  TrxSchedule sche1(blks, modes);
  auto rlp = sche1.rlp();
  TrxSchedule sche2(rlp);
  EXPECT_EQ(sche1, sche2);
}

TEST(PbftManager, create_pbft_manager) {
  PbftManager pbft_mgr;
  pbft_mgr.start();
  thisThreadSleepForSeconds(2);
  pbft_mgr.stop();
}
}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  // logOptions.verbosity = dev::VerbosityError;
  // logOptions.includeChannels.push_back("PBFT_MGR");

  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}