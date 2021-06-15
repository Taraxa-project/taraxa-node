from dataclasses import dataclass

from common.node import Node
from common.wait import wait


@dataclass(frozen=True)
class TestHelper:
    node_executable_path: str

    def start_nodes(self, *configs):
        nodes = [Node(self.node_executable_path, cfg) for cfg in configs]
        min_peers_to_connect_to = len(nodes) // 2
        print("waiting for the nodes to connect...")
        wait(lambda: all(min_peers_to_connect_to <= node.w3_http.net.peer_count for node in nodes))
        print("nodes connected!")
        return nodes
