from random import randint
from typing import Iterable

from common.node.node import Node
from common.util.wait import wait


class Cluster:
    RANDOM_NODE_INDEX = -1

    def __init__(self, nodes: Iterable[Node], wait_interconnect=True):
        self._nodes = list(nodes)
        assert 0 < len(self._nodes)
        self.default_node_index = Cluster.RANDOM_NODE_INDEX
        if wait_interconnect:
            self.wait_adequate_interconnection()

    # assumption: after this method is called the object is never used again
    def destructor(self):
        for node in self:
            node.destructor()

    @staticmethod
    def from_new_nodes(config_and_wallet_path_pairs, init_mode: Node.InitMode, wait_interconnect=True):
        return Cluster((Node(cfg_path, wallet_path, genesis_path, init_mode)
                        for cfg_path, wallet_path, genesis_path in config_and_wallet_path_pairs),
                       wait_interconnect=wait_interconnect)

    def __iter__(self):
        return iter(self._nodes)

    def __len__(self):
        return len(self._nodes)

    def use_w3_provider(self, provider_type: str):
        for node in self:
            node.use_w3_provider(provider_type)

    def node(self, index=None):
        index = self.default_node_index if index is None else index
        return self.random_node if index == Cluster.RANDOM_NODE_INDEX else self._nodes[index]

    @property
    def random_node_index(self):
        return randint(0, len(self._nodes) - 1)

    @property
    def random_node(self):
        return self._nodes[self.random_node_index]

    def wait_adequate_interconnection(self):
        min_peers_to_connect_to = len(self._nodes) // 2
        print(f"waiting for the nodes to connect to each other (min peers per node: {min_peers_to_connect_to})...")
        wait(lambda: all(min_peers_to_connect_to <= n.net.peer_count for n in self))
        print("nodes connected!")
