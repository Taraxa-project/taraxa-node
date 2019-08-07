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
import os
from create_conf import create_taraxa_conf

# local test

START_BOOT_NODE = "./build-d/main --boot_node true --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE = "./build-d/main --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE_SILENT = "./build-d/main --conf_taraxa py_test/conf/conf_taraxa{}.json -v 0 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"

NODE_IP = "127.0.0.1"
NODE_PORTS = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786, 
              7787, 7788, 7789, 7790, 7791, 7792, 7793, 7794, 7795, 7796]
TOTAL_TARAXA_COINS = 9007199254740991
TOTAL_TRXS = 50000
NODE_BAL = 50000
BOOT_NODE_SECRET_KEY = "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"
BOOT_NODE_ADDRESS = "de2b1203d72d3549ee2f733b00b2789414c7cea5"

def read_account_table(file_name):
    node_secret=[]
    node_public=[]
    node_address=[]
    with open(file_name, "r") as f:
        counter = 0
        for line in f:
            data = line.rstrip('\n')
            if counter==1:
                node_secret.append(data)
            elif counter==2:
                node_public.append(data)
            elif counter==3:
                node_address.append(data)
            counter= (counter+1)%4
    return node_secret, node_public, node_address

def rpc(NODE_IP, node_port, data):
    request = None
    try:
        request = requests.post("http://{}:{}".format(NODE_IP, node_port), data=json.dumps(data))
    except Exception as e:
        print(e)
    return request

def send_get_peer_count(NODE_IP, node_port):
    get_peer_count_data = {
        "jsonrpc": "2.0",
        "id": node_port,
        "method": "get_peer_count",
        "params": [{}]
    }

    res = rpc(NODE_IP, node_port, get_peer_count_data)
    if res == None or res.status_code != 200:
        print("Send get_peer_count failed ...")
    else:
        print("Response: ", res.text)

    return res.json()["result"]["value"]

def start_full_node(node):
    if (node == 0):
        cmd = START_BOOT_NODE.format(node, node, node)
    else:
        cmd = START_FULL_NODE.format(node, node, node)
    subprocess.call(cmd, shell=True)

def start_full_node_process(node):
    p = multiprocessing.Process(target=start_full_node, args=(node,))
    p.start()
    time.sleep(1)

def start_multi_full_nodes(num_nodes):
    jobs = []
    for node in range(0, num_nodes):
        p = multiprocessing.Process(target=start_full_node, args=(node,))
        jobs.append(p)
        p.start()
        time.sleep(1)

def all_nodes_connected(num_nodes):
    while (True):
        total_peers = 0
        ok = 1
        for node in range(num_nodes):
            peers = send_get_peer_count(NODE_IP, NODE_PORTS[node])
            if (peers<=2):
                ok = 0
        if ok:
            return
        time.sleep(2)

def get_account_address(NODE_IP, node_port):
    account_address_data = {
        "action": "get_account_address"
    }
    print account_address_data

    res = rpc(NODE_IP, node_port, account_address_data)
    if res == None or res.status_code != 200 :
        print("Send get_account_address failed ...")
    else:
        print("Response: ", res.text)

    return res.text

def get_account_balance(NODE_IP, node_port, account_address):
    request_data = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_account_balance",
        "params": [{
            "address": account_address,
        }]
    }
    res = rpc(NODE_IP, node_port, request_data)
    if res == None or res.status_code != 200 :
        print("Send get_account_balance failed ...")
    else: 
        json_reply = json.loads(res.text)
        result = json_reply["result"]
        balance = result["value"]
        found = result["found"]
        # print("Account: ", address,", Balance: ", balance, "Found: ", found)
        return int(balance)
      


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

def send_trx(NODE_IP, node_port, trx):
    r = rpc(NODE_IP, node_port, trx)
    if r == None or r.status_code != 200 :
        print("Send trx failed ...")
    else:
        print("Response: ", r.text)

def send_coins(transfer_ip, transfer_port, receiver, value):
    nonce = 0
    send_trx(transfer_ip, transfer_port, send_coins_trx(nonce, value, receiver, BOOT_NODE_SECRET_KEY))

def send_many_trx_to_neighbor(node_port, neighbor, number_of_trx_created):
    try:
        request = jsonrpc_cmd_create_many_coin_trx(0, neighbor, number_of_trx_created)
        json_reply = rpc(NODE_IP, node_port, request)
        print (json_reply)
    except Exception as e:
        print(e) 

def jsonrpc_cmd_create_many_coin_trx(nonce, recv, number): 
    n = int(number)
    request = {"jsonrpc":"2.0", \
            "id":0, \
            "method": "create_test_coin_transactions", \
            "params":[{ \
                "delay": 1, \
                "number": int(number), \
                "nonce": int(nonce), \
                "receiver": recv}] \
            }
    return request

def send_trx_from_node_to_neighbors_testing(num_nodes):
    global NODE_BAL
    number_of_trx_created = int(TOTAL_TRXS/num_nodes)
    total_transfer = 100 * number_of_trx_created
    print ("total_transfer ", total_transfer)
    for receiver in range(num_nodes):
        neighbor = NODE_ADDRESS[receiver+1]
        print("Node", receiver, "create", number_of_trx_created, "trxs to (",receiver+1,")" ,neighbor, ",total coins=", total_transfer)
        send_many_trx_to_neighbor(NODE_PORTS[receiver], neighbor, number_of_trx_created)
    ok = 1
    print("Wait for coin transfer ...")
    for i in range(10):
        time.sleep(num_nodes)
        ok = 1
        for receiver in range(num_nodes): # loop through different nodes
            if not ok:
                break
            for acc in range(num_nodes+1): # loop through different accounts
                expected_bal = 0
                if (acc == 0):
                    continue
                    #expected_bal = 9007199254740991 - total_transfer
                elif (acc==num_nodes):
                    expected_bal = total_transfer
                else: 
                    expected_bal = NODE_BAL
                account = NODE_ADDRESS[acc]
                bal = get_account_balance(NODE_IP, NODE_PORTS[receiver], account)
                if bal != expected_bal:
                    print("Error! node", receiver, "account (", acc,")", account, "balance =", bal, "Expected =", expected_bal)
                    ok = 0
                    break
        if ok: 
            break
    if ok:
        print("Coin transfer done ...")
    else: 
        print("Coin transfer failed ...")
    return ok


def balance_check_test(num_nodes):
    start_multi_full_nodes(num_nodes)

    time.sleep(5)
    all_nodes_connected(num_nodes)
    coins = NODE_BAL
    for i in range (num_nodes):
        # transfer coins from boot node to other address
        if i == 0:
            continue
        send_coins(NODE_IP, NODE_PORTS[0], NODE_ADDRESS[i], coins)
    time.sleep(20)

    for i in range (num_nodes):
        get_account_balance(NODE_IP, NODE_PORTS[i], NODE_ADDRESS[i])
    send_trx_from_node_to_neighbors_testing(num_nodes)

def main():
    num_nodes = 15
    for path in glob.glob("/tmp/taraxa*"):
        shutil.rmtree(path, ignore_errors=True)
    for path in glob.glob("./logs/node*"):
        shutil.rmtree(path, ignore_errors=True)
    for path in glob.glob("./py_test/conf/conf_*"):
        shutil.rmtree(path, ignore_errors=True)
    if not os.path.exists("./logs"):
        os.makedirs("./logs")
    global NODE_ADDRESS, NODE_PUBLIC, NODE_SECRET
    NODE_SECRET, NODE_PUBLIC, NODE_ADDRESS = read_account_table("./core_tests/account_table.txt")
    create_taraxa_conf(num_nodes, NODE_SECRET, NODE_PUBLIC[0])
    balance_check_test(num_nodes)
    sys.exit()

if __name__ == "__main__":
    main()