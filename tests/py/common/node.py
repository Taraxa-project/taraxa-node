import io
import json
import os
import shutil
import tempfile
import time
from pathlib import Path
from subprocess import Popen
from typing import Optional, Callable, Any

import web3
from web3 import Web3


class Node:

    def __init__(self, executable_path, config_dict_or_file_path=None):
        if isinstance(config_dict_or_file_path, dict):
            self._cfg_path = None
            self.cfg = config_dict_or_file_path
        else:
            self._cfg_path = config_dict_or_file_path
            with open(self._cfg_path, mode="r") as f:
                self.cfg = json.load(f)
        self.account = web3.Account.from_key(self.cfg["node_secret"])
        self.data_path = Path(self.cfg["db_path"])
        self.w3_http: Optional[Web3] = None
        self.w3_ws: Optional[Web3] = None
        self._executable_path = Path(executable_path)
        self._proc, self._running = None, False
        self.start()

    @property
    def running(self):
        return self._running

    def start(self):
        if self._running:
            return False
        self._running = True
        cfg_file_path = self._cfg_path
        if self._cfg_path is None:
            cfg_file_descriptor, cfg_file_path = tempfile.mkstemp()
            with io.open(cfg_file_descriptor, mode='w') as cfg_file:
                json.dump(self.cfg, cfg_file)
        self._proc = Popen([self._executable_path, "--conf_taraxa", cfg_file_path])
        if self._cfg_path is None:
            os.remove(cfg_file_path)
        time.sleep(1)
        cfg_rpc = self.cfg.get("rpc", {})
        rpc_http_port = cfg_rpc.get("http_port", None)
        if rpc_http_port is not None:
            self.w3_http = Web3(Web3.HTTPProvider(endpoint_uri=f"http://127.0.0.1:{rpc_http_port}"))
        rpc_ws_port = cfg_rpc.get("ws_port", None)
        if rpc_ws_port is not None:
            self.w3_ws = Web3(Web3.WebsocketProvider(endpoint_uri=f"ws://127.0.0.1:{rpc_ws_port}"))
        return True

    def with_all_w3_backends(self, action: Callable[[Web3], Any]):
        return [action(backend) for backend in [self.w3_http, self.w3_ws]]

    def terminate(self):
        if not self._running:
            return False
        self._running = False
        return self._proc.terminate()


def cleanup_data(*nodes: Node):
    for node in nodes:
        assert not node.running
        shutil.rmtree(node.data_path, ignore_errors=True)


def terminate_all(*nodes: Node, _cleanup_data=True):
    for node in nodes:
        node.terminate()
        if _cleanup_data:
            cleanup_data(node)
