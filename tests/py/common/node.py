import atexit
import io
import json
import os
import shutil
import tempfile
from pathlib import Path
from subprocess import Popen
from typing import Optional, Callable, Any

import web3
from eth_account.signers.local import LocalAccount
from web3 import Web3

from common.wait import wait


class Node:

    def __init__(self, executable_path, config_dict_or_file_path, clean_data=True):
        if isinstance(config_dict_or_file_path, dict):
            self._cfg_path = None
            self.cfg = config_dict_or_file_path
        else:
            self._cfg_path = config_dict_or_file_path
            with open(self._cfg_path, mode="r") as f:
                self.cfg = json.load(f)
        self.account: LocalAccount = web3.Account.from_key(self.cfg["node_secret"])
        self.data_path = Path(self.cfg["db_path"])
        self.w3_http: Optional[Web3] = None
        self.w3_ws: Optional[Web3] = None
        self._executable_path = Path(executable_path)
        self._proc, self._running = None, False
        atexit.register(self.terminate)
        self.start(clean_data=clean_data)

    @property
    def running(self):
        return self._running

    def start(self, clean_data=True):
        if self._running:
            return False
        self._running = True
        if clean_data:
            shutil.rmtree(self.data_path, ignore_errors=True)
        cfg_file_path = self._cfg_path
        if self._cfg_path is None:
            cfg_file_descriptor, cfg_file_path = tempfile.mkstemp()
            with io.open(cfg_file_descriptor, mode='w') as cfg_file:
                json.dump(self.cfg, cfg_file)
        self._proc = Popen([self._executable_path, "--conf_taraxa", cfg_file_path])
        if self._cfg_path is None:
            os.remove(cfg_file_path)
        cfg_rpc = self.cfg.get("rpc", {})
        rpc_http_port = cfg_rpc.get("http_port", None)
        if rpc_http_port is not None:
            self.w3_http = Web3(Web3.HTTPProvider(
                endpoint_uri=f"http://127.0.0.1:{rpc_http_port}",
                # request_kwargs=dict(timeout=60),
            ))
        rpc_ws_port = cfg_rpc.get("ws_port", None)
        if rpc_ws_port is not None:
            self.w3_ws = Web3(Web3.WebsocketProvider(
                endpoint_uri=f"ws://127.0.0.1:{rpc_ws_port}",
                # websocket_timeout=60,
            ))
        self.with_all_w3_backends(lambda w3: wait(lambda: w3.net.listening))
        return True

    def with_all_w3_backends(self, action: Callable[[Web3], Any]):
        res = []
        for backend in [self.w3_http, self.w3_ws]:
            if backend is not None:
                try:
                    res.append(action(backend))
                except Exception as e:
                    raise Exception(f"Exception for w3 provider type: {type(backend.provider)}") from e
        return res

    def terminate(self):
        if not self._running:
            return False
        self._running = False
        self._proc.terminate()
        return True


def terminate_all(*nodes: Node):
    for node in nodes:
        node.terminate()
