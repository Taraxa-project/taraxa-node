namespace taraxa::cli {

const char *testnet_json = R"foo({
  "node_secret": "",
  "vrf_secret": "",
  "data_path": "",
  "network_is_boot_node": false,
  "network_address": "0.0.0.0",
  "network_tcp_port": 10002,
  "network_udp_port": 10002,
  "network_simulated_delay": 0,
  "network_transaction_interval": 100,
  "network_encrypted": 1,
  "network_bandwidth": 40,
  "network_ideal_peer_count": 5,
  "network_max_peer_count": 15,
  "network_sync_level_size": 25,
  "network_boot_nodes": [
    {
      "id": "f36f467529fe91a750dfdc8086fd0d2f30bad9f55a5800b6b4aa603c7787501db78dc4ac1bf3cf16e42af7c2ebb53648653013c3da1987494960d751871d598a",
      "ip": "boot-node-0.testnet.taraxa.io",
      "tcp_port": 10002,
      "udp_port": 10002
    },
    {
      "id": "d2d445fc3276bdbf2a07628a484baabf45ccb91d6aa078a0dc3aefc11f1941899a923312ec0e664b8e57b63b774c47a006d9d1d16befd2135dcf76067736c688",
      "ip": "boot-node-1.testnet.taraxa.io",
      "tcp_port": 10002,
      "udp_port": 10002
    },
    {
      "id": "c6e7263f44d88c0d6cc3b0d5ebdc31cf891908e8fa7e545e137d3ed0bfec1810fa24c1379228afbb53df0d59e716e17138115fd096782a84261718ab77665171",
      "ip": "boot-node-2.testnet.taraxa.io",
      "tcp_port": 10002,
      "udp_port": 10002
    }
  ],
  "rpc": {
    "http_port": 7777,
    "ws_port": 8777,
    "threads_num": 10
  },
  "test_params": {
    "max_transaction_queue_warn": 0,
    "max_transaction_queue_drop": 0,
    "max_block_queue_warn": 0,
    "db_snapshot_each_n_pbft_block": 100,
    "db_max_snapshots": 5,
    "block_proposer": {
      "shard": 1,
      "transaction_limit": 250
    }
  },
  "logging": {
    "configurations": [
      {
        "name": "standard",
        "on": true,
        "verbosity": "ERROR",
        "channels": [
          {
            "name": "SUMMARY",
            "verbosity": "INFO"
          }
        ],
        "outputs": [
          {
            "type": "console",
            "format": "%TimeStamp% %Channel% %SeverityStr% %Message%"
          },
          {
            "type": "file",
            "file_name": "Taraxa_N1_%m%d%Y_%H%M%S_%5N.log",
            "rotation_size": 10000000,
            "time_based_rotation": "0,0,0",
            "format": "%NodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%",
            "max_size": 1000000000
          }
        ]
      }
    ]
  },
  "chain_config": {
    "chain_id": "0x2",
    "dag_genesis_block": {
      "level": "0x0",
      "pivot": "0x0000000000000000000000000000000000000000000000000000000000000000",
      "sig": "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
      "timestamp": "0x60b0329a",
      "tips": [],
      "transactions": []
    },
    "final_chain": {
      "genesis_block_fields": {
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x60b0329a"
      },
      "state": {
        "disable_block_rewards": true,
        "dpos": {
          "deposit_delay": "0x5",
          "withdrawal_delay": "0x5",
          "eligibility_balance_threshold": "0xf4240",
          "genesis_state": {
            "0x6c05d6e367a8c798308efbf4cefc1a18921a6f89": {
              "0x18551e353aa65bc0ffbdf9d93b7ad4a8fe29cf95": "0xf4240",
              "0xc578bb5fc3dac3e96a8c4cb126c71d2dc9082817": "0xf4240",
              "0x5c9afb23fba3967ca6102fb60c9949f6a38cd9e8": "0xf4240"
            }
          }
        },
        "eth_chain_config": {
          "byzantium_block": "0x0",
          "constantinople_block": "0x0",
          "dao_fork_block": "0xffffffffffffffff",
          "eip_150_block": "0x0",
          "eip_158_block": "0x0",
          "homestead_block": "0x0",
          "petersburg_block": "0x0"
        },
        "execution_options": {
          "disable_gas_fee": false,
          "disable_nonce_check": true
        },
        "genesis_balances": {
          "6c05d6e367a8c798308efbf4cefc1a18921a6f89": "0x1027e72f1f12813088000000",
          "f4a52b8f6dc8ab046fec6ad02e77023c044342e4": "0x1027e72f1f12813088000000"
        }
      }
    },
    "pbft": {
      "committee_size": "0x3e8",
      "dag_blocks_size": "0x32",
      "ghost_path_move_back": "0x0",
      "lambda_ms_min": "0x3e8",
      "run_count_votes": false
    },
    "replay_protection_service": {
      "range": "0xa"
    },
    "vdf": {
      "difficulty_max": "0x12",
      "difficulty_min": "0x10",
      "threshold_selection": "0xbffd",
      "threshold_vdf_omit": "0x6bf7",
      "difficulty_stale": "0x13",
      "lambda_bound": "0x64"
    }
  }
}
)foo";
}