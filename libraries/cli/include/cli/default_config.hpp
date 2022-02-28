namespace taraxa::cli {

const char *default_json = R"foo({
  "node_secret": "",
  "vrf_secret": "",
  "data_path": "",
  "network_is_boot_node": false,
  "network_listen_ip": "0.0.0.0",
  "network_tcp_port": 10002,
  "network_udp_port": 10002,
  "network_simulated_delay": 0,
  "network_transaction_interval": 100,
  "network_bandwidth": 40,
  "network_ideal_peer_count": 10,
  "network_max_peer_count": 50,
  "network_sync_level_size": 10,
  "network_packets_processing_threads": 14,
  "network_peer_blacklist_timeout" : 600,
  "deep_syncing_threshold" : 10,
  "network_boot_nodes": [
    {
      "id": "d063098ceca0f5ea06f9455debffe6f6d5b2efdeb179215877e356cf8154afad99f058214bd25d8198a3854a4ed8f7ef97af59b0441a7d30bc4b3918c42764ef",
      "ip": "boot-node-0.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "572778c6636361916acbc596808e1aa4e93ee0b577188f594015ec42125f58b7ec75fd59d78ca2282bc1c8c639dfc4d0eac2e57855b4fcc932c6bc6f530cd8f6",
      "ip": "boot-node-1.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "10a5108d0070559cacbb0ed368d43c98848fbc0397e9bfc1a6e7e33f15532a804d158e405657650d3ea766ed4b0d7cfcbe5867e9261deacb457da332ed1eaf2a",
      "ip": "boot-node-2.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "1dd6ae9674dee4e30f2f2f76036ad01be34bb617095b0513bc3818c8352e040fc3648fd400896fc80a544b3c23118e143928d0d49d4d575b68327f70da342939",
      "ip": "boot-node-3.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "56b74f83d3e88319cf554c6e43ca6077f7cf9594547e530582ed12196dd891b289dc7b5b4392440bcead5adbdf4e23e184b7f46940efd603c5bf0a25b36d71c6",
      "ip": "boot-node-4.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "833a3a724f5be3654524cb83307628117a668a00808fb2f34c43e5e3cb1a30354536cbdc9355172e5f285e1b4b3dde88d94385fa4df69c89da0638760ed5b0ac",
      "ip": "boot-node-5.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "94026c10fce281ede4bae9b54606703eba77cb876ddacb3eba364b7e444618d424641de206f5d5cb39f5c04da48ebad502b76d698ed341e46721c8777a8c3f15",
      "ip": "boot-node-6.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "03947eaba01564a69d6ea238f4c264ee50b34918838fee164b1b0f0de833b0098f2d4a7c05448a0f3d3aa05d55fe4d150aa3a1553fd940a433433aff8ae57a0e",
      "ip": "boot-node-7.mainnet.taraxa.io",
      "udp_port": 10002
    },
    {
      "id": "8a3394cccb379d66e6b7b131ef8ea9f195c333020b246135bf5d888443e8b6245cef9c90f2b6466240f38c9cfa4ccaa472b0f204623f09da819068c6d1168242",
      "ip": "boot-node-8.mainnet.taraxa.io",
      "udp_port": 10002
    }
  ],
  "rpc": {
    "http_port": 7777,
    "ws_port": 8777,
    "threads_num": 10
  },
  "test_params": {
    "max_transactions_pool_warn": 0,
    "max_transactions_pool_drop": 0,
    "max_block_queue_warn": 0,
    "db_snapshot_each_n_pbft_block": 10000,
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
            "format": "%ThreadID% %Channel% [%TimeStamp%] %SeverityStr%: %Message%"
          },
          {
            "type": "file",
            "file_name": "Taraxa_N1_%m%d%Y_%H%M%S_%5N.log",
            "rotation_size": 10000000,
            "time_based_rotation": "0,0,0",
            "format": "%ThreadID% %ShortNodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%",
            "max_size": 1000000000
          }
        ]
      },
      {
        "name": "network",
        "on": false,
        "verbosity": "ERROR",
        "channels": [
          {
            "name": "PBFT_CHAIN",
            "verbosity": "INFO"
          },
          {
            "name": "PBFT_MGR",
            "verbosity": "DEBUG"
          },
          {
            "name": "GET_PBFT_SYNC_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "PBFT_SYNC_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "GET_DAG_SYNC_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "DAG_SYNC_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "DAG_BLOCK_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "PBFT_BLOCK_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "TARCAP",
            "verbosity": "DEBUG"
          },
          {
            "name": "NETWORK",
            "verbosity": "DEBUG"
          },
          {
            "name": "TRANSACTION_PH",
            "verbosity": "DEBUG"
          },
          {
            "name": "DAGBLKMGR",
            "verbosity": "INFO"
          },
          {
            "name": "DAGMGR",
            "verbosity": "INFO"
          }
        ],
        "outputs": [
          {
            "type": "console",
            "format": "%ThreadID% %Channel% [%TimeStamp%] %SeverityStr%: %Message%"
          },
          {
            "type": "file",
            "file_name": "TaraxaNetwork_N1_%m%d%Y_%H%M%S_%5N.log",
            "rotation_size": 10000000,
            "time_based_rotation": "0,0,0",
            "format": "%ThreadID% %ShortNodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%",
            "max_size": 1000000000
          }
        ]
      }
    ]
  },
  "chain_config": {
    "chain_id": "0x1",
    "dag_genesis_block": {
      "level": "0x0",
      "pivot": "0x0000000000000000000000000000000000000000000000000000000000000000",
      "sig": "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
      "timestamp": "0x61CD9F40",
      "tips": [],
      "transactions": []
    },
    "final_chain": {
      "genesis_block_fields": {
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x61CD9F40"
      },
      "state": {
        "disable_block_rewards": true,
        "dpos": {
          "deposit_delay": "0x5",
          "withdrawal_delay": "0x5",
          "eligibility_balance_threshold": "0x186A0",
          "vote_eligibility_balance_step": "0x186A0",
          "genesis_state": {
            "0x0274cfffea9fa850e54c93a23042f12a87358a82": {
              "0x1f8333245650a19a0683891b7afe7787a3ce9f00": "0x989680",
              "0xd4e4728bea5a67dd70dccb742bdc9c3a48465bec": "0x989680",
              "0xec591a85f613fe98f057dc09712a9b22cdd05845": "0x989680",
              "0x267e780b7843992e57f52e13018f0f97467ac06e": "0x989680",
              "0x9d047654e55248ec38aa6723a5ab36d171008584": "0x989680",
              "0x0d149e61cc02b5893ef6fc33bc7d67ff13eeeee0": "0x989680",
              "0x00ccd0de0809ac03fd292036ee1544185583cd88": "0x989680",
              "0x6f96be7626a74e86c76e65ccbccf0a38e2b62fc5": "0x989680",
              "0xd20131f980c9932b1df31cf3aafeecfb1d504381": "0x989680"
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
          "0274cfffea9fa850e54c93a23042f12a87358a82": "0x141e8d17",
          "111f91441efc8c6c0edf6534970cc887e2fabaa8": "0x24048ce3d"
        },
        "hardforks": {
          "fix_genesis_fork_block": "0x0"
        }
      }
    },
    "pbft": {
      "committee_size": "0x3e8",
      "number_of_proposers": "0x14",
      "dag_blocks_size": "0xa",
      "ghost_path_move_back": "0x0",
      "lambda_ms_min": "0x5dc",
      "run_count_votes": false
    },
    "replay_protection_service": {
      "range": "0xa"
    },
    "sortition": {
      "changes_count_for_average": "0x5",
      "max_interval_correction": "0x3E8",
      "dag_efficiency_targets": ["0x12C0", "0x1450"],
      "changing_interval": "0x0",
      "computation_interval": "0xC8",
      "vrf": {
        "threshold_upper": "0x1770",
        "threshold_range": "0xbb8"
      },
      "vdf": {
        "difficulty_max": "0x12",
        "difficulty_min": "0x10",
        "difficulty_stale": "0x14",
        "lambda_bound": "0x64"
      }
    }
  }
}
)foo";
}