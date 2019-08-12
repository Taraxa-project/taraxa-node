import glob
import json
import os

CONF_DIR = "py_test/conf"


def create_taraxa_conf(num_conf, secrets, boot_node_pk, boot_node_addr):
    if not os.path.exists(CONF_DIR):
        os.makedirs(CONF_DIR)
    for i in range(num_conf):
        conf = {
            "node_secret": secrets[i],
            "db_path": "/tmp/taraxa"+str(i),
            "dag_processing_threads": 1,
            "network_address": "127.0.0.1",
            "network_listen_port": 10002 + i,
            "network_simulated_delay": 0,
            "network_transaction_interval": 100,
            "network_bandwidth": 40,
            "network_boot_nodes": [
                {
                    "id": boot_node_pk,
                    "ip": "127.0.0.1",
                    "port": 10002
                }
            ],
            "network_id": "testnet",
            "rpc_port": 7777+i,
            "ws_port": 8777+i,
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
                    "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
                    "hash": "0000000000000000000000000000000000000000000000000000000000000000",
                    "sender": "",
                    "timestamp": 1234567890
                },
                "accounts": {
                    boot_node_addr: {
                        "balance": 9007199254740991
                    }
                }
            }
        }
        f = open(CONF_DIR+"/conf_taraxa"+str(i)+".json", "w")
        f.write(json.dumps(conf, indent=2))
        f.close()
