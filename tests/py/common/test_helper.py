import time
from dataclasses import dataclass

from common.node import Node


@dataclass(frozen=True)
class TestHelper:
    node_executable_path: str
    start_nodes_connect_check_attempts: int = 30
    start_nodes_connect_check_period_sec: int = 2

    def start_nodes(self, *configs):
        nodes = [Node(self.node_executable_path, cfg) for cfg in configs]
        min_peers_to_connect_to = len(nodes) // 2
        attempts_left = self.start_nodes_connect_check_attempts
        while not all(min_peers_to_connect_to <= node.w3_http.net.peer_count for node in nodes):
            if attempts_left <= 0:
                raise TimeoutError()
            print("waiting for nodes to connect...")
            time.sleep(self.start_nodes_connect_check_period_sec)
            attempts_left -= 1
        print("nodes connected!")
        return nodes
