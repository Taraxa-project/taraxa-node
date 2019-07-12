//
// Created by JC on 2019-05-14.
//

/* This file serves as a demo for using prometheus c++ client assuming the
 * push-gateway and prometheus are available. To test the prometheus cpp
 * locally, follow the steps below:
 * ---------------------------  prometheus cpp client:
 * --------------------------- The prometheus cpp is expected to be built
 * through MakeFile's git submodule; run instructions below if makefile fails
 * Step 1: mkdir -p submodules/prometheus-cpp/
 * Step 2: in the directory, git clone
 * https://github.com/jupp0r/prometheus-cpp.git Step 3: build prometheus_cpp
 * client following the Building section and 'via CMake' sub-section at
 *         https://github.com/jupp0r/prometheus-cpp
 * ---------------------------  pushgateway  ---------------------------
 * Step 4: download pushgateway binary from
 * https://prometheus.io/download/#pushgateway Step 5: run push gateway at
 * localhost, set port 9091: './pushgateway --web.listen-address=0.0.0.0:9091'
 * Step 6: open a browser and access localhost:9091/
 * ---------------------------  prometheus itself  ---------------------------
 * // TODO: current promethes was not connected with the push-gateway
 * Step 7: download prometheus from https://prometheus.io/download/ ; the tar
 * file contains an executable 'prometheus' Step 8: config 'prometheus.yml'
 * (more info:
 * https://prometheus.io/docs/prometheus/latest/configuration/configuration/)
 * Step 9: start the prometheus with './prometheus --config.file=prometheus.yml'
 * */

#include <gtest/gtest.h>
#include <string>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "libdevcore/Log.h"
#include "prometheus/gateway.h"
#include "prometheus/registry.h"
#include "prometheus/summary.h"
#include "util.hpp"
using namespace std;

namespace taraxa {
class PrometheusDemo : public testing::Test {};
string push_gateway_name = "pushgateway";
string push_gateway_ip = "0.0.0.0";
string push_gateway_port = "9091";
static std::string GetHostName() {
  char hostname[1024];
  if (::gethostname(hostname, sizeof(hostname))) {
    return {};
  }
  return hostname;
}

/* This is the simplest demo for a counter that increments every second modified
 * from sample push client
 * https://github.com/jupp0r/prometheus-cpp/blob/4e0814ee3f93b796356a51a4795a332568940a72/push/tests/integration/sample_client.cc
 * */
// TEST(PrometheusDemo, DISABLED_prometheus_gateway) {
TEST(PrometheusDemo, prometheus_gateway) {
  // create a link to push gateway
  std::string hostName = GetHostName();
  const auto labels = prometheus::Gateway::GetInstanceLabel(hostName);

  // Connect to gateway running at ip = 'localhost' (i.e. "0.0.0.0"), port =
  // '9091'. The job name is 'pushgateway' with instance name in the labels
  prometheus::Gateway gateway{push_gateway_ip, push_gateway_port,
                              push_gateway_name, labels};
  // create a metrics registry with component=main labels applied to all its
  // metrics
  auto registry = std::make_shared<prometheus::Registry>();

  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  auto& counter_family = prometheus::BuildCounter()
                             .Name("time_running_seconds_total")
                             .Help("How many seconds is this server running?")
                             .Labels({{"label", "value"}})
                             .Register(*registry);
  // add a counter to the metric family
  auto& second_counter = counter_family.Add(
      {{"another_label", "value"}, {"yet_another_label", "value"}});

  // add a new counter family to the registry
  auto& counter_family_even =
      prometheus::BuildCounter()
          .Name("count_of_even_i")
          .Help("How many seconds does event 'even i' happened?")
          .Labels({{"label", "value"}})
          .Register(*registry);
  // add another event counter to the metric family
  auto& even_event_counter = counter_family_even.Add(
      {{"another_label", "value"}, {"yet_another_label", "value"}});

  // ask the pusher to push the metrics to the pushgateway
  gateway.RegisterCollectable(registry);

  for (int i = 0; i < 10; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // increment the counter by one (second)
    second_counter.Increment();
    if (i % 2) {
      even_event_counter.Increment();
    }
    // push request shall return 200.
    EXPECT_EQ(gateway.Push(), 200);
  }
}

void timerTestTarget(int millis) {
  // do nothing; emulate the delayMillis
  std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}
void timerTestTargetFast(int millis) {
  // do nothing; emulate the delayMillis
  std::this_thread::sleep_for(std::chrono::milliseconds(millis / 2));
}

/* This is the demo for a timer that measure a function in {50%, 90%, 99%}
 * percentile
 * */
TEST(PrometheusDemo, prometheus_timer) {
  const int num_func = 10, max_delay = 1000;
  std::vector<std::thread> gateways;

  // create a link to push gateway
  std::string hostName = GetHostName();
  const auto labels = prometheus::Gateway::GetInstanceLabel(hostName);
  // Connect to gateway running at ip = 'localhost' (i.e. "0.0.0.0"), port =
  // '9091'. The job name is 'pushgateway' with instance name in the labels
  prometheus::Gateway gateway{push_gateway_ip, push_gateway_port,
                              push_gateway_name, labels};
  // create a metrics registry with component=main labels applied to all its
  // metrics
  auto registry = std::make_shared<prometheus::Registry>();

  // add a new summary family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  auto& summary_family = prometheus::BuildSummary()
                             .Name("prometheus_timer")
                             .Help(
                                 "How many seconds does it take for the "
                                 "function timerTestTarget to run?")
                             .Labels({{"label", "value"}})
                             .Register(*registry);

  auto& summary_family_fast = prometheus::BuildSummary()
                                  .Name("prometheus_timer_fast")
                                  .Help(
                                      "How many seconds does it take for the "
                                      "function timerTestTargetFast to run?")
                                  .Labels({{"label", "value"}})
                                  .Register(*registry);

  // ask the pusher to push the metrics to the pushgateway
  gateway.RegisterCollectable(registry);

  for (auto idx = 0; idx < num_func; ++idx) {
    gateways.emplace_back([&gateway, &labels, &summary_family, idx, this]() {
      // add a summary for {50%, 90%, 99%} percentile of the time taken
      auto& second_summary = summary_family.Add(
          {{"another_label", "value"}, {"yet_another_label", "value"}},
          prometheus::Summary::Quantiles{
              {0.5, 0.05}, {0.90, 0.01}, {0.99, 0.001}});
      for (int delay = 10; delay <= max_delay; delay += 10) {
        // record the local time before our Timer-Test-Target starts; note the
        // precision is set to microsecond
        boost::posix_time::ptime t1 =
            boost::posix_time::microsec_clock::local_time();

        // we want to know how long the target function takes
        // the test-target delays (i.e. runs) {10ms, 20ms ... 1000 ms}
        // we therefore expect to see 50% at ~500ms, 90% at ~900ms and 99% at
        // ~990ms
        timerTestTarget(delay);

        boost::posix_time::ptime t2 =
            boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_duration diff = t2 - t1;
        // record the summary in ms
        second_summary.Observe(diff.total_milliseconds());
        EXPECT_EQ(gateway.Push(), 200);
      }
    });

    // I know this is ugly, but shall be fine for a demo testing
    gateways.emplace_back(
        [&gateway, &labels, &summary_family_fast, idx, this]() {
          // add a summary for {50%, 90%, 99%} percentile of the time taken
          auto& second_summary = summary_family_fast.Add(
              {{"another_label", "value"}, {"yet_another_label", "value"}},
              prometheus::Summary::Quantiles{
                  {0.5, 0.05}, {0.90, 0.01}, {0.99, 0.001}});
          for (int delay = 10; delay <= max_delay; delay += 10) {
            // record the local time before our Timer-Test-Target starts; note
            // the precision is set to microsecond
            boost::posix_time::ptime t1 =
                boost::posix_time::microsec_clock::local_time();

            // we want to know how long the target function takes
            // the test-target delays (i.e. runs) {5ms, 10ms ... 500 ms} i.e.
            // twice as fast as timeTestTarget runs we therefore expect to see
            // 50% at ~250ms, 90% at ~450ms and 99% at ~500ms
            timerTestTargetFast(delay);

            boost::posix_time::ptime t2 =
                boost::posix_time::microsec_clock::local_time();
            boost::posix_time::time_duration diff = t2 - t1;
            // record the summary in ms
            second_summary.Observe(diff.total_milliseconds());
            EXPECT_EQ(gateway.Push(), 200);
          }
        });
  }
  // for(int i = 0; i < num_func; i++) {
  for (auto& f : gateways) {
    if (f.joinable()) {
      f.join();
    }
  }
}

TEST(PrometheusDemo, prometheus_counter_multi_thread) {
  // TEST(PrometheusDemo, DISABLED_prometheus_counter_multi_thread) {
  const int num_metrics = 10, cnt = 100;
  std::vector<std::thread> gateways;
  // create a link to push gateway
  std::string hostName = GetHostName();
  const auto labels = prometheus::Gateway::GetInstanceLabel(hostName);
  // Connect to gateway running at ip = 'localhost' (i.e. "0.0.0.0"), port =
  // '9091'. The job name is 'pushgateway' with instance name in the labels
  prometheus::Gateway gateway{push_gateway_ip, push_gateway_port,
                              push_gateway_name, labels};
  // create a metrics registry with component=main labels applied to all its
  // metrics
  auto registry = std::make_shared<prometheus::Registry>();
  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  auto& counter_family = prometheus::BuildCounter()
                             .Name("prometheus_counter_multi_thread")
                             .Help("How many times does the target happen?")
                             .Labels({{"label", "value"}})
                             .Register(*registry);

  // ask the pusher to push the metrics to the pushgateway. Note the regisry and
  // gateway is shared among different thread
  gateway.RegisterCollectable(registry);

  for (auto idx = 0; idx < num_metrics; ++idx) {
    // add a counter to the metric family
    gateways.emplace_back([&gateway, &labels, &counter_family, idx, this]() {
      auto& second_counter = counter_family.Add(
          {{"another_label", "value"}, {"yet_another_label", "value"}});
      for (int j = 0; j < cnt; j++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // increment the counter by one
        second_counter.Increment();
        EXPECT_EQ(gateway.Push(), 200);
      }
    });
  }
  for (int i = 0; i < num_metrics; i++) {
    if (gateways[i].joinable()) {
      gateways[i].join();
    }
  }
}

}  // namespace taraxa

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbosityWarning;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  if (argc == 4) {
    taraxa::push_gateway_ip = argv[1];
    taraxa::push_gateway_port = argv[2];
    taraxa::push_gateway_name = argv[3];
  } else if (argc != 1) {
    cerr << "ERROR - usage: make pdemo [push_gateway_ip push_gateway_port "
            "push_gateway_name] \n";
    return 1;
  }
  return RUN_ALL_TESTS();
}