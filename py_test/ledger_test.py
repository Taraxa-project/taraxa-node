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
import psutil
from create_conf import create_taraxa_conf
from taraxa_rpc import *
# local test

START_BOOT_NODE = "./build/main --rebuild_network true --boot_node true --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE = "./build/main --rebuild_network true --conf_taraxa py_test/conf/conf_taraxa{}.json -v 2 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"
START_FULL_NODE_SILENT = "./build/main --conf_taraxa py_test/conf/conf_taraxa{}.json -v 0 --log-channels FULLND PBFT_CHAIN PBFT_MGR VOTE_MGR SORTI EXETOR > ./logs/node{}.out 2>&1"

NODE_PORTS = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786,
              7787, 7788, 7789, 7790, 7791, 7792, 7793, 7794, 7795, 7796]
BOOT_NODE_PORT = NODE_PORTS[0]
TOTAL_TARAXA_COINS = 9007199254740991
NUM_TRXS = 100
INIT_NODE_BAL = 90000


def get_arguments():
    [num_nodes] = sys.argv[1:]
    return int(num_nodes)


def test_fail():
    print "****** [Test Fail] ******"
    return 0


def test_success():
    print "****** [Test Success] ******"
    return 1


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
    print "Create ", num_nodes, "nodes"
    jobs = []
    for node in range(0, num_nodes):
        p = multiprocessing.Process(target=start_full_node, args=(node,))
        jobs.append(p)
        p.start()
        time.sleep(1)
    return jobs


def all_nodes_connected(num_nodes):
    for i in range(10):
        ok = 1
        for node in range(num_nodes):
            peers = taraxa_rpc_get_peer_count(NODE_PORTS[node])
            if (peers == 0):
                ok = 0
        if ok:
            return 1
        time.sleep(1)
    return 0


def send_get_boot_node_balance():
    return taraxa_rpc_get_account_balance(BOOT_NODE_PORT, BOOT_NODE_ADDR)


def trx_count_testing(num_nodes):
    # use first node as answer
    answer = 0
    ok = 1
    for node in range(num_nodes):
        count = taraxa_rpc_get_transaction_count(NODE_PORTS[node])
        if (node == 0):
            answer = count
            print("Trx count =", count)
        elif (count != answer):
            print("Error! node", node, "has only",
                  count, " transactions, expected:", answer)
            ok = 0
        print("Check transaction count ... in node", node, "status =", ok)
    return ok


def dag_size_testing(num_nodes):
    # use first node as answer
    answer = 0
    ok = 1
    for node in range(num_nodes):
        sz = taraxa_rpc_get_dag_size(NODE_PORTS[node])
        if (node == 0):
            answer = sz
            print("Dag size =", sz)
        elif (sz != answer):
            print("Error! node", node, "has only",
                  sz, "DAG, expected:", answer)
            ok = 0
        print("Check dag size ... in node", node, "status =", ok)
    return ok


def initialize_coin_allocation(num_nodes, coins):
    boot_node_coin = send_get_boot_node_balance()
    print("Boot node balance", boot_node_coin)
    global INIT_NODE_BAL
    print("Expected init balance", INIT_NODE_BAL)
    for i in range(num_nodes):
        neighbor = NODE_ADDRESS[i+1]
        taraxa_rpc_send_coins(BOOT_NODE_PORT, neighbor, INIT_NODE_BAL)
    print("Wait for coin init ...")
    ok = 1
    for counter in range(50):
        time.sleep(1)
        ok = 1
        for node in range(num_nodes):  # loop through different nodes
            if not ok:
                break
            for acc in range(num_nodes):  # loop through different accounts
                account = NODE_ADDRESS[acc]
                bal = taraxa_rpc_get_account_balance(
                    NODE_PORTS[node], account)
                expected_bal = 0
                if (acc == 0):
                    expected_bal = TOTAL_TARAXA_COINS - \
                        INIT_NODE_BAL*(num_nodes)
                else:
                    expected_bal = INIT_NODE_BAL
                if bal != expected_bal:
                    print("Warning! node", node, "account(", acc, ")",
                          account, "balance =", bal, "Expected =", expected_bal)
                    ok = 0
                    break
        if ok:
            break
    if ok:
        print("Coin init done ...")
    else:
        print("Coin init failed ...")
    return ok


def send_trx_from_node_to_neighbors_testing(num_nodes):
    global INIT_NODE_BAL
    number_of_trx_created = int(NUM_TRXS)
    total_transfer = 100 * number_of_trx_created
    print("total_transfer ", total_transfer)
    for sender in range(num_nodes):
        receiver = sender + 1
        neighbor = NODE_ADDRESS[receiver]
        print("Node", NODE_PORTS[sender], "create", number_of_trx_created, "trxs to (",
              receiver, ")", neighbor, ",total coins=", total_transfer)
        taraxa_rpc_send_many_trx_to_neighbor(
            NODE_PORTS[sender], neighbor, number_of_trx_created)
    ok = 1
    print("Wait for coin transfer ...")
    for i in range(50):
        time.sleep(num_nodes)
        ok = 1
        for node in range(num_nodes):  # loop through different nodes
            if not ok:
                break
            for acc in range(num_nodes+1):  # loop through different accounts
                expected_bal = 0
                if (acc == 0):
                    expected_bal = TOTAL_TARAXA_COINS - \
                        (num_nodes*INIT_NODE_BAL) - total_transfer
                elif (acc == num_nodes):
                    expected_bal = INIT_NODE_BAL+total_transfer
                else:
                    expected_bal = INIT_NODE_BAL
                account = NODE_ADDRESS[acc]
                bal = taraxa_rpc_get_account_balance(
                    NODE_PORTS[node], account)
                if bal != expected_bal:
                    print("Error! node", node, "account (", acc, ")",
                          account, "balance =", bal, "Expected =", expected_bal)
                    ok = 0
                    trx_count_testing(num_nodes)
                    dag_size_testing(num_nodes)
                    break
        if ok:
            break
    if ok:
        print("Coin transfer done ...")
    else:
        print("Coin transfer failed ...")
    return ok


def balance_check_test(num_nodes):

    time.sleep(3)
    ok = all_nodes_connected(num_nodes)
    if not ok:
        return 0
    ok = initialize_coin_allocation(num_nodes, INIT_NODE_BAL)
    if not ok:
        return 0
    ok = send_trx_from_node_to_neighbors_testing(num_nodes)
    return ok


def killtree(pid, including_parent=True):
    parent = psutil.Process(pid)
    for child in parent.children(recursive=True):
        print "Killing chilld process", child
        child.kill()
    if including_parent:
        print "Kill process", parent
        parent.kill()


def terminate_full_nodes(jobs):
    pids = []
    for j in jobs:
        killtree(j.pid)


def main():

    num_nodes = get_arguments()
    # delete previous results
    for path in glob.glob("/tmp/taraxa*"):
        shutil.rmtree(path, ignore_errors=False)
    for path in glob.glob("./logs"):
        shutil.rmtree(path, ignore_errors=False)
    for path in glob.glob("./py_test/conf"):
        shutil.rmtree(path, ignore_errors=False)
    if not os.path.exists("./logs"):
        os.makedirs("./logs")
    global NODE_ADDRESS, NODE_PUBLIC, NODE_SECRET
    NODE_SECRET, NODE_PUBLIC, NODE_ADDRESS = read_account_table(
        "./core_tests/account_table.txt")

    create_taraxa_conf(num_nodes, NODE_SECRET, BOOT_NODE_PK, BOOT_NODE_ADDR)

    jobs = start_multi_full_nodes(num_nodes)

    ok = balance_check_test(num_nodes)

    terminate_full_nodes(jobs)
    # sys.exit()
    if ok:
        test_success()
    else:
        test_fail()


if __name__ == "__main__":
    main()
