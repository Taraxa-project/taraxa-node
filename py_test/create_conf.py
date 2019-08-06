import glob
import json
import os

CONF_DIR = "conf"
def create_taraxa_conf (num_conf, secret, boot_node_pk):
  if not os.path.exists(CONF_DIR):
    os.makedirs(CONF_DIR)
  for i in range(num_conf):
    conf = { 
      "node_secret": secret,
      "db_path": "tmp/taraxa"+str(i),
      "dag_processing_threads": 1, 
      "network_address": "0.0.0.0", 
      "network_listen_port": 10002 + i,
      "network_simulated_delay": 0, 
      "network_transaction_interval": 100,
      "network_bandwidth": 40,
      "network_boot_nodes": [
        {
          "id": boot_node_pk,
          "ip": "0.0.0.0",
          "port": 10002
        }
      ],
      "network_id": "testnet",
      "rpc_port": 7777+i, 
      "test_params": {
        "block_proposer": [
          0,
          1,
          100,
          1000
        ],
        "pbft": [
          1000,
          1,
          90000
        ]
      },
      "genesis_state": {
        "account_start_nonce": 0,
        "block": {
          "level": 0,
          "tips": [],
          "trxs": [],
          "sig": "",
          "hash": "0000000000000000000000000000000000000000000000000000000000000000",
          "sender": ""
        },
        "accounts": {
          "de2b1203d72d3549ee2f733b00b2789414c7cea5": {
            "balance": 9007199254740991
          }
        }
      }
    }
    f = open(CONF_DIR+"/conf_taraxa"+str(i)+".json", "w")
    f.write(json.dumps(conf, indent=2))
    f.close()