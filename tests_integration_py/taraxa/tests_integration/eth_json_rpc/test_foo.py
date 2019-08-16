from web3 import Web3, HTTPProvider
from taraxa.tests_integration.common import make_node_cfg_file


def test_bar():
    w3 = Web3(HTTPProvider('http://localhost:7777'))
    config_file, _ = make_node_cfg_file(base_config={
        "dag_processing_threads": 1,
        "network_address": "0.0.0.0",
        "network_listen_port": 10002,
        "network_simulated_delay": 0,
        "network_transaction_interval": 100,
        "network_bandwidth": 40,
        "network_boot_nodes": [
            {
                "id": "7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4623a14a421d"
                      "4f030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27b627412a",
                "ip": "127.0.0.1",
                "port": 10002
            }
        ],
        "network_id": "testnet",
        "rpc_port": 7777,
        "test_params": {
            "block_proposer": [
                0,
                1,
                100,
                1000
            ],
            "pbft": [
                1000,
                3,
                10000
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
                "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
                "sender": ""
            },
            "accounts": {
                "de2b1203d72d3549ee2f733b00b2789414c7cea5": {
                    "balance": 9007199254740991
                }
            }
        }
    })
    pass
