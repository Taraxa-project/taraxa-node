#!/usr/bin/python

import glob
import json
import multiprocessing
import random
import requests
import shutil
import subprocess
import sys
import time


# local test
START_FULL_NODE = "./build/main --conf_taraxa ./core_tests/conf/conf_taraxa{}.json -v 3 --log-filename logs/node{}.log --log-channels PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI"
node_ip = "0.0.0.0"
nodes_port = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786]
TOTAL_TARAXA_COINS = 9007199254740991
TRXS_NUM = 100
BOOT_NODE_SECRET_KEY = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"

def rpc(node_ip, node_port, data):
    request = None
    try:
        request = requests.post("http://{}:{}".format(node_ip, node_port), data=json.dumps(data))
    except Exception as e:
        print(e)
    return request

def send_get_peer_count(node_ip, node_port):
    get_peer_count_data = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_peer_count",
        "params": [{}]
    }

    res = rpc(node_ip, node_port, get_peer_count_data)
    if res == None or res.status_code != 200:
        print("Send get_peer_count failed ...")
    else:
        print("Response: ", res.text)

    return res.json()["result"]

def start_full_node(node):
    cmd = START_FULL_NODE.format(node, node)
    print cmd
    subprocess.call(cmd, shell=True)

def start_multi_full_nodes(nodes_number):
    jobs = []
    for node in range(1, nodes_number + 1):
        p = multiprocessing.Process(target=start_full_node, args=(node,))
        jobs.append(p)
        p.start()
        time.sleep(1)

def get_peer_count(nodes_number):
    while (True):
        total_peers = 0
        for node in range(nodes_number):
            peers = send_get_peer_count(node_ip, nodes_port[node])
            total_peers += int(peers)
        if (total_peers == (nodes_number - 1) * nodes_number):
            return
        time.sleep(2)

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

def get_account_balance(node_ip, node_port, account_address):
    request_data = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_account_balance",
        "params": [{
            "address": account_address,
        }]
    }
    print request_data

    res = rpc(node_ip, node_port, request_data)
    if res == None or res.status_code != 200 :
        print("Send get_account_balance failed ...")
    else:
        print("Response: ", res.text)

def set_boot_node_account_balance():
    account_address = get_account_address(node_ip, nodes_port[0])
    set_account_balance(node_ip, nodes_port[0], account_address, TOTAL_TARAXA_COINS)

def set_nodes_account_balance(nodes_number):
    for node_index in range(nodes_number):
        account_address = get_account_address(node_ip, nodes_port[node_index])
        for node in range(nodes_number):
            print "set account balance in ", node_ip, " port ", nodes_port[node]
            set_account_balance(node_ip, nodes_port[node], account_address, TOTAL_TARAXA_COINS/nodes_number)

def send_coins_trx(nonce, value, receiver, secret_key):
    trx = {
        "jsonrpc": "2.0",
        "id": "1",
        "method": "send_coin_transaction",
        "params": [{
            "nonce": nonce,
            "value": value,
            "receiver": receiver,
            "secret": secret_key,
        }]
    }
    print trx
    return trx

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

def send_trx(node_ip, node_port, trx):
    r = rpc(node_ip, node_port, trx)
    if r == None or r.status_code != 200 :
        print("Send trx failed ...")
    else:
        print("Response: ", r.text)

def generate_test_trx_for_each_node(nodes_number):
    while (True):
        random.seed()
        for node in range(nodes_number):
            print "Send create transaction ... to node ", node_ip, " port ", nodes_port[node]
            #send_trx(get_node_name(receiver), create_trx(receiver, TOTAL_TRXS/nodes_number))
            seed = random.randint(0, 1000)
            print "seed: ", seed
            send_trx(node_ip, nodes_port[node], create_trx(seed, TRXS_NUM))
            time.sleep(2)

def send_coins(transfer_ip, transfer_port, receiver, value):
    nonce = 0
    send_trx(transfer_ip, transfer_port, send_coins_trx(nonce, value, receiver, BOOT_NODE_SECRET_KEY))

def main():
    nodes_number = int(sys.argv[1])
    for path in glob.glob("/tmp/taraxa*"):
        shutil.rmtree(path, ignore_errors=True)
    for path in glob.glob("./logs/node*"):
        shutil.rmtree(path, ignore_errors=True)
    start_multi_full_nodes(nodes_number)

    get_peer_count(nodes_number)

    boot_node_address = "de2b1203d72d3549ee2f733b00b2789414c7cea5"
    node_address2 = "973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0"
    node_address3 = "4fae949ac2b72960fbe857b56532e2d3c8418d5e"
    node_address4 = "415cf514eb6a5a8bd4d325d4874eae8cf26bcfe0"
    node_address5 = "b770f7a99d0b7ad9adf6520be77ca20ee99b0858"

    coins = TOTAL_TARAXA_COINS / 4
    send_coins(node_ip, nodes_port[0], node_address2, coins)
    send_coins(node_ip, nodes_port[0], node_address3, coins)
    send_coins(node_ip, nodes_port[0], node_address4, coins)
    time.sleep(30)

    # boot_node_address = get_account_address(node_ip, nodes_port[0])
    # node_address1 = get_account_address(node_ip, nodes_port[1])
    # node_address2 = get_account_address(node_ip, nodes_port[2])
    coins = 1
    for i in range(1000):
        # transfer coins from boot node to address1
        send_coins(node_ip, nodes_port[0], node_address5, coins)
        coins += 1
        # for i in range(nodes_number):
        get_account_balance(node_ip, nodes_port[0], boot_node_address)
        get_account_balance(node_ip, nodes_port[1], node_address2)
        get_account_balance(node_ip, nodes_port[2], node_address3)
        get_account_balance(node_ip, nodes_port[3], node_address4)
        get_account_balance(node_ip, nodes_port[4], node_address5)
        time.sleep(15)

#    generate_test_trx_for_each_node(nodes_number)

if __name__ == "__main__":
    main()