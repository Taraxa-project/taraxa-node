#pragma once

#include <string_view>

namespace taraxa::core_tests {

constexpr std::string_view test_json = R"foo({
  "network_listen_ip": "127.0.0.1",
  "network_transaction_interval": 100,
  "network_ideal_peer_count": 10,
  "network_max_peer_count": 50,
  "network_sync_level_size": 1,
  "network_packets_processing_threads": 5,
  "network_boot_nodes": [
    {
      "ip": "127.0.0.1",
      "id": "7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4623a14a421d4f030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27b627412a",
      "tcp_port": 10003,
      "udp_port": 10003
    }
  ],
  "rpc": {
    "threads_num": 2
  },
  "max_transactions_pool_warn": 0,
  "max_transactions_pool_drop": 0,
  "max_block_queue_warn": 0,
  "logging": {
    "configurations": [
      {
        "name": "standard",
        "on": true,
        "verbosity": "ERROR",
        "channels": [],
        "outputs": [
          {
            "type": "console",
            "format": "%ThreadID% %ShortNodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%"
          },
          {
            "type": "file",
            "rotation_size": 10000000,
            "time_based_rotation": "0,0,0",
            "format": "%ThreadID% %ShortNodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%",
            "max_size": 1000000000
          }
        ]
      }
    ]
  }
}
)foo";
}