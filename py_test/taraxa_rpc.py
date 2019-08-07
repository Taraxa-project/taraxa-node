import json
import requests

BOOT_NODE_SK = "14ef9fd11e54900b587e29e490acca5ded1b1e1d91dcf431307e66e5ade2d0ce"
BOOT_NODE_ADDRESS = "036b2a833f806894f6da127634572947736b04a3"
BOOT_NODE_PK = "f0981293a51c244bdcef2e49d46437a113170a0875c61641bf8e41e50cc4a362aa131cef09d83c9d599e76a27ad61d9a4482e76ff1406967f08de7e960f57c87"


def rpc(node_port, data):
    node_name = "127.0.0.1"
    try:
        reply = requests.post(
            "http://{}:{}".format(node_name, node_port), data=json.dumps(data))
        if reply == None or reply.status_code != 200 or "error" in reply.text:
            print("Send rpc failed ...", reply.text)
            return
    except Exception as e:
        print(e)
    json_reply = json.loads(reply.text)
    # print("JSON REPLY ", json_reply)
    return json_reply


def taraxa_rpc_get_peer_count(node_port):
    request = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_peer_count",
        "params": [{}]
    }
    try:
        json_reply = rpc(node_port, request)
        peer = json_reply["result"]["value"]
        print("Node ", node_port, "has", peer, "peers")
        return int(peer)
    except Exception as e:
        print(e)


def taraxa_rpc_get_account_balance(node_port, account_address):
    request = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_account_balance",
        "params": [{
            "address": account_address,
        }]
    }
    try:
        json_reply = rpc(node_port, request)
        result = json_reply["result"]
        balance = result["value"]
        found = result["found"]
        # print("Account: ", address,", Balance: ", balance, "Found: ", found)
        return int(balance)
    except Exception as e:
        print(e)


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
    try:
        json_reply = rpc(node_port, request)
        print("Boot node send", value, "coins to", receiver)
    except Exception as e:
        print(e)


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
    try:
        json_reply = rpc(node_port, request)
        print("Node", node_port, "send",
              number_of_trx_created, "trxs to", neighbor)
    except Exception as e:
        print(e)
