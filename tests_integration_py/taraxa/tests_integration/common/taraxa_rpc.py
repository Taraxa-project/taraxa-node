import json
import requests

BOOT_NODE_SK = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"
BOOT_NODE_PK = "7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4623a14a421d4f" \
               "030cf25ef368ab394a45e920e14b57a259a09c41767dd50d1da27b627412a"
BOOT_NODE_ADDR = "de2b1203d72d3549ee2f733b00b2789414c7cea5"


def rpc(node_port, data):
    node_name = "127.0.0.1"
    reply = requests.post(
        "http://{}:{}".format(node_name, node_port), data=json.dumps(data))
    if reply is None or reply.status_code != 200 or "error" in reply.text:
        print("Send rpc failed ...", reply.text)
        return
    json_reply = json.loads(reply.text)
    # print("JSON REPLY ", json_reply)
    return json_reply


def taraxa_rpc_get_transaction_count(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_transaction_count",
        "params": [{}]
    }
    json_reply = rpc(node_port, request)
    count = json_reply["result"]["value"]
    return count


def taraxa_rpc_get_dag_size(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_dag_size",
        "params": [{}]
    }
    json_reply = rpc(node_port, request)
    dag_size = json_reply["result"]["value"].split(",")
    # print(("DAG size:", dag_size[1]))
    return dag_size


def taraxa_rpc_get_peer_count(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_peer_count",
        "params": [{}]
    }
    json_reply = rpc(node_port, request)
    peer = json_reply["result"]["value"]
    print("Node ", node_port, "has", peer, "peers")
    return int(peer)


def taraxa_rpc_get_account_balance(node_port, account_address):
    request = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_account_balance",
        "params": [{
            "address": account_address,
        }]
    }
    json_reply = rpc(node_port, request)
    result = json_reply["result"]
    balance = result["value"]
    found = result["found"]
    # print("Account: ", address,", Balance: ", balance, "Found: ", found)
    return int(balance)


def taraxa_rpc_send_coins(node_port, receiver, value):
    request = {
        "jsonrpc": "2.0",
        "id": "1",
        "method": "send_coin_transaction",
        "params": [{
            "nonce": 0,
            "value": value,
            "receiver": receiver,
            "secret": BOOT_NODE_SK,
        }]
    }
    json_reply = rpc(node_port, request)
    print("Boot node send", value, "coins to", receiver)


def taraxa_rpc_send_many_trx_to_neighbor(node_port, neighbor, number_of_trx_created):
    request = {
        "jsonrpc": "2.0",
        "id": 0,
        "method": "create_test_coin_transactions",
        "params": [{
            "delay": 1,
            "number": int(number_of_trx_created),
            "nonce": 0,
            "receiver": neighbor}]
    }
    json_reply = rpc(node_port, request)
    # print("Node", node_port, "send",
    # number_of_trx_created, "trxs to", neighbor)


def taraxa_rpc_get_executed_trx_count(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_executed_trx_count",
        "params": [{}]
    }
    json_reply = rpc(node_port, request)
    result = json_reply["result"]
    count = result["value"]
    return int(count)


def taraxa_rpc_get_executed_blk_count(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_executed_blk_count",
        "params": [{}]
    }
    json_reply = rpc(node_port, request)
    result = json_reply["result"]
    count = result["value"]
    return int(count)

# Below are taraxa rpc


def taraxa_rpc_blockNumber(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "taraxa_blockNumber",
        "params": []
    }
    json_reply = rpc(node_port, request)
    block_num = json_reply["result"]
    return int(block_num, 0)
