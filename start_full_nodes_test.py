#!/usr/bin/python

import glob
import json
import multiprocessing
import requests
import shutil
import subprocess
import sys
import time


# local test
START_FULL_NODE = "./build/main --conf_taraxa ./core_tests/conf_taraxa{}.json -v 3 --log-filename node{}.log --log-channels PBFT_CHAIN PBFT_MGR"
node_ip = "0.0.0.0"
nodes_port = [7777, 7778, 7779, 7780, 7781, 7782, 7783, 7784, 7785, 7786]

def rpc(node_ip, node_port, data):
    request = None
    try:
        request = requests.post("http://{}:{}".format(node_ip, node_port), data=json.dumps(data))
    except Exception as e:
        print(e)
    return request

def send_get_peer_count(node_ip, node_port):
    get_peer_count_data = {
        "action": "get_peer_count"
    }
    print get_peer_count_data

    res = rpc(node_ip, node_port, get_peer_count_data)
    if res == None or res.status_code != 200:
        print("Send get_peer_count failed ...")
    else:
        print("Response: ", res.text)

    return res.text

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
            print "node ", node, " peers number ", peers
            total_peers += int(peers)
        if (total_peers == (nodes_number - 1) * nodes_number):
            return
        time.sleep(2)

def main():
    nodes_number = int(sys.argv[1])
    for path in glob.glob("/tmp/taraxa*"):
        shutil.rmtree(path, ignore_errors=True)
    start_multi_full_nodes(nodes_number)
    get_peer_count(nodes_number)

if __name__ == "__main__":
    main()