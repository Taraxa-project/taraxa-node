import json


def create_taraxa_conf(path_fn, num_conf, secrets, boot_node_pk, boot_node_addr):
    for i in range(num_conf):
        conf = {
            "node_secret": secrets[i],
            "db_path": "/tmp/taraxa" + str(i),
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
                    2000,
                    1,
                    1000000000,
                    6000
                ]
            },
            "genesis_state": {
                "account_start_nonce": 0,
                "block": {
                    "level": 0,
                    "tips": [],
                    "trxs": [],
                    "sig": "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
                    "hash": "c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69",
                    "sender": "de2b1203d72d3549ee2f733b00b2789414c7cea5",
                    "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
                    "timestamp": 1564617600
                },
                "accounts": {
                    boot_node_addr: {
                        "balance": 9007199254740991
                    }
                }
            }
        }
        with open(path_fn(i), "w") as f:
            f.write(json.dumps(conf, indent=2))
