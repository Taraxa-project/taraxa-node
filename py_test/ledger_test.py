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
from taraxa_rpc import *
# local test

START_BOOT_NODE = "./build/main --boot_node true --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE = "./build/main --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE_SILENT = "./build/main --conf_taraxa py_test/conf/conf_taraxa{}.json -v 0 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"

NODE_PORTS = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786,
              7787, 7788, 7789, 7790, 7791, 7792, 7793, 7794, 7795, 7796]
TOTAL_TARAXA_COINS = 9007199254740991
TOTAL_TRXS = 50000
NODE_BAL = 50000


def read_account_table(file_name):
    node_secret = []
    node_public = []
    node_address = []
    with open(file_name, "r") as f:
        counter = 0
        num_acc = 0
        for line in f:
            data = line.rstrip('\n')
            if counter == 1:
                node_secret.append(data)
            elif counter == 2:
                node_public.append(data)
            elif counter == 3:
                node_address.append(data)
            else:
                num_acc += 1
            counter = (counter+1) % 4
    print("Number of accounts read:", num_acc)
    return node_secret, node_public, node_address


def start_full_node(node):
    if (node == 0):
        cmd = START_BOOT_NODE.format(node, node, node)
    else:
        cmd = START_FULL_NODE.format(node, node, node)
    print(cmd)
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
    return jobs


def terminate_full_nodes(jobs):
    for j in jobs:
        j.terminate()
        j.join()


def all_nodes_connected(num_nodes):
    while (True):
        ok = 1
        for node in range(num_nodes):
            peers = taraxa_rpc_get_peer_count(NODE_PORTS[node])
            if (peers == 0):
                ok = 0
        if ok:
            return
        time.sleep(2)


def send_trx_from_node_to_neighbors_testing(num_nodes):
    global NODE_BAL
    number_of_trx_created = int(TOTAL_TRXS/num_nodes)
    total_transfer = 100 * number_of_trx_created
    print("total_transfer ", total_transfer)
    for receiver in range(num_nodes):
        neighbor = NODE_ADDRESS[receiver+1]
        print("Node", receiver, "create", number_of_trx_created, "trxs to (",
              receiver+1, ")", neighbor, ",total coins=", total_transfer)
        taraxa_rpc_send_many_trx_to_neighbor(
            NODE_PORTS[receiver], neighbor, number_of_trx_created)
    ok = 1
    print("Wait for coin transfer ...")
    for i in range(10):
        time.sleep(num_nodes)
        ok = 1
        for receiver in range(num_nodes):  # loop through different nodes
            if not ok:
                break
            for acc in range(num_nodes+1):  # loop through different accounts
                expected_bal = 0
                if (acc == 0):
                    continue
                    #expected_bal = 9007199254740991 - total_transfer
                elif (acc == num_nodes):
                    expected_bal = total_transfer
                else:
                    expected_bal = NODE_BAL
                account = NODE_ADDRESS[acc]
                bal = taraxa_rpc_get_account_balance(
                    NODE_PORTS[receiver], account)
                if bal != expected_bal:
                    print("Error! node", receiver, "account (", acc, ")",
                          account, "balance =", bal, "Expected =", expected_bal)
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

    time.sleep(5)
    all_nodes_connected(num_nodes)
    coins = NODE_BAL
    for i in range(num_nodes):
        # transfer coins from boot node to other address
        taraxa_rpc_send_coins(NODE_PORTS[0], NODE_ADDRESS[i], coins)
    time.sleep(20)

    for i in range(num_nodes):
        taraxa_rpc_get_account_balance(NODE_PORTS[i], NODE_ADDRESS[i])
    send_trx_from_node_to_neighbors_testing(num_nodes)


def main():

    num_nodes = 3
    # delete previous results
    for path in glob.glob("/tmp/taraxa*"):
        shutil.rmtree(path, ignore_errors=True)
    for path in glob.glob("./logs/node*"):
        shutil.rmtree(path, ignore_errors=True)
    for path in glob.glob("./py_test/conf/conf_*"):
        shutil.rmtree(path, ignore_errors=True)
    if not os.path.exists("./logs"):
        os.makedirs("./logs")
    global NODE_ADDRESS, NODE_PUBLIC, NODE_SECRET
    NODE_SECRET, NODE_PUBLIC, NODE_ADDRESS = read_account_table(
        "./core_tests/account_table.txt")

    jobs = start_multi_full_nodes(num_nodes)

    create_taraxa_conf(num_nodes, NODE_SECRET, BOOT_NODE_PK)
    balance_check_test(num_nodes)

    terminate_full_nodes(jobs)
    sys.exit()


if __name__ == "__main__":
    main()
