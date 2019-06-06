#!/usr/bin/python

import requests
import json
import sys
import time
import random

MAIN_NODE_ID = 0
TRXS_NUM = 100
TOTAL_TARAXA_COINS = 9007199254740991
INSERT_BLOCK_SEPARATOR = "Block stamped at"
SEND_TRX_SEPARATOR = "Transaction"
GET_BLOCK_SEPARATOR = "time_stamp"
TEMPLATE_NODE_NAME = "taraxa-node-{}.taraxa-node.qi-test"
# local test
node_ip = "0.0.0.0"
nodes_port = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786]

def create_trx(seed, number):
    n = int(number)
    sd = int(seed*121+73)
    sk = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dce"
    trx = {"action": "create_test_coin_transactions", \
           "secret": sk, \
           "delay": 1, \
           "number": n, \
           "seed": sd
           }
    print trx
    return trx

def get_peer_count(node_ip, node_port):
    get_peer_count_data = {
        "action": "get_peer_count"
    }
    print get_peer_count_data

    res = rpc(node_ip, node_port, get_peer_count_data)
    if res == None or res.status_code != 200 :
        print("Send get_peer_count failed ...")
    else:
        print("Response: ", res.text)

    return res.text

def get_account_address(node_ip, node_port):
    account_address_data = {
        "action": "get_account_address"
    }
    print account_address_data

    res = rpc(node_ip, node_port, account_address_data)
    if res == None or res.status_code != 200 :
        print("Send get_account_address failed ...")
    else:
        print("Response: ", res.text)

    return res.text

def set_account_balance(node_ip, node_port, account_address, account_balance):
    account_balance_data = {
        "action": "set_account_balance",
        "address": account_address,
        "balance": account_balance
    }
    print account_balance_data

    res = rpc(node_ip, node_port, account_balance_data)
    if res == None or res.status_code != 200 :
        print("Send set_account_balance failed ...")
    else:
        print("Response: ", res.text)

def get_arguments():
    [mode, number_nodes] = sys.argv[1:]
    return (int(mode), int(number_nodes),)

def get_node_name(identification):
    return TEMPLATE_NODE_NAME.format(identification)

def check_http_response_send_trx(request):
    if request == None or request.status_code != 200 or SEND_TRX_SEPARATOR not in request.text:
        print("The transaction was not send successfully")
        exit(1)

def rpc(node_ip, node_port, data):
    request = None
    try:
        request = requests.post("http://{}:{}".format(node_ip, node_port), data=json.dumps(data))
    except Exception as e:
        print(e)
    return request

def get_dictionary_from_request(request, separator):
    result = None
    if request == None:
        return result
    print(request.text)
    try:
        [block, _] = request.text.split(separator)
        result = json.loads(block)
    except Exception as e:
        print(e)
    return result

def get_inserted_block(node, hash):
    payload = {'action': 'get_dag_block', 'hash':hash}
    r = rpc(node, payload)
    return get_dictionary_from_request(r, GET_BLOCK_SEPARATOR)


def send_trx(node_ip, node_port, trx):
    r = rpc(node_ip, node_port, trx)
    if r == None or r.status_code != 200 :
        print("Send trx failed ...")
    else:
        print("Response: ", r.text)
    # check_http_response_send_trx(r)
    # return get_dictionary_from_request(r, SEND_TRX_SEPARATOR)

def check_http_response_send_trx(nodes_number, inserted_block):
    wrong_nodes = []
    for i in range(1, nodes_number):
        node = get_node_name(i)
        print("Checking Node: ", node)
        block = get_inserted_block(node, inserted_block["hash"])
        if block != inserted_block:
            wrong_nodes.append(node)
    return wrong_nodes

def test_failed():
    print("Test Failed.")
    time.sleep(2)
    exit(1)

def get_peer_count_test(nodes_number):
    while (True):
        for node in range(nodes_number):
            peers = get_peer_count(node_ip, nodes_port[node])
            print "node ", node, " peers number ", peers
        time.sleep(2)

def set_account_balance_test(nodes_number):
    for node_index in range(nodes_number):
        print "get account address"
        account_address = get_account_address(node_ip, nodes_port[node_index])
        for node in range(nodes_number):
            print "set account balance ", node_ip, " port ", nodes_port[node]
            set_account_balance(node_ip, nodes_port[node], account_address, TOTAL_TARAXA_COINS/nodes_number)

def generate_trx_on_one_node_once_test():
    seed = random.randint(0, 1000)
    send_trx(node_ip, nodes_port[0], create_trx(seed, 30000))

def generate_trx_for_each_node_test(nodes_number):
    while (True):
        random.seed()
        for node in range(nodes_number):
            print "Send create transaction ... to node ", node_ip, " port ", nodes_port[node]
            #send_trx(get_node_name(receiver), create_trx(receiver, TOTAL_TRXS/nodes_number))
            seed = random.randint(0, 1000)
            print "seed: ", seed
            send_trx(node_ip, nodes_port[node], create_trx(seed, TRXS_NUM))
            time.sleep(1)

def main():
    nodes_number = int(sys.argv[1])
    set_account_balance_test(nodes_number)
    generate_trx_for_each_node_test(nodes_number)

if __name__ == "__main__":
    main()

#./build/main --conf_taraxa ./core_tests/conf_taraxa1.json -v 3 --log-filename node1.log --log-channels PBFT_CHAIN PBFT_MGR TARCAP
