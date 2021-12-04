#include "pbft/timing_machine.hpp"

#include <gtest/gtest.h>

#include <thread>

#include "common/static_init.hpp"
#include "util_test/util.hpp"

namespace taraxa::core_tests {

struct TimingMachineTest : BaseTest {};

std::string node_sk_string = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd";
Secret node_sk = dev::Secret(node_sk_string, dev::Secret::ConstructFromStringType::FromHex);
KeyPair node_key = dev::KeyPair(node_sk);
const Address& node_addr = node_key.address();

void timeoutFunc(std::shared_ptr<TimingMachine> timing_machine, time_stamp_t end_time) {
  timing_machine->setTimeOut(end_time);
  timing_machine->timeOut();
}

void wakeupFunc(std::shared_ptr<TimingMachine> timing_machine, time_stamp_t wakeup_ms) {
  taraxa::thisThreadSleepForMilliSeconds(wakeup_ms);
  timing_machine->wakeUp();
}

TEST(TimingMachineTest, timeout_test) {
  const time_stamp_t sleep_ms = 2000;

  TimingMachine timing_machine(node_addr);
  timing_machine.initialClockInNewRound();
  auto now = std::chrono::system_clock::now();
  timing_machine.setTimeOut(sleep_ms);
  timing_machine.timeOut();
  auto duration = std::chrono::system_clock::now() - now;
  auto sleep_time = static_cast<time_stamp_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
  // Add 50ms for system time
  EXPECT_LT(sleep_time, sleep_ms + 50);
  std::cout << "Sleep time: " << sleep_time << std::endl;
  timing_machine.stop();
}

TEST(TimingMachineTest, wakeup_test) {
  const time_stamp_t sleep_ms = 3000;
  const time_stamp_t wakeup_ms = 1000;

  auto timing_machine = std::make_shared<TimingMachine>(node_addr);
  timing_machine->initialClockInNewRound();
  auto now = std::chrono::system_clock::now();

  std::thread timeout_thread(timeoutFunc, timing_machine, sleep_ms);
  std::thread wakeup_thread(wakeupFunc, timing_machine, wakeup_ms);
  timeout_thread.join();
  wakeup_thread.join();

  auto duration = std::chrono::system_clock::now() - now;
  auto sleep_time = static_cast<time_stamp_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
  // Add 50ms for system time
  EXPECT_LT(sleep_time, wakeup_ms + 50);
  std::cout << "Sleep time: " << sleep_time << std::endl;
  timing_machine->stop();
}

}  // namespace taraxa::core_tests

using namespace taraxa;
int main(int argc, char** argv) {
  taraxa::static_init();
  auto logging = logger::createDefaultLoggingConfig();
  logging.verbosity = logger::Verbosity::Error;
  logging.channels["TIMING_MACHINE"] = logger::Verbosity::Error;

  addr_t node_addr;
  logger::InitLogging(logging, node_addr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}