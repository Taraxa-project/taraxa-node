import atexit
import json
import shutil
from dataclasses import dataclass
from pathlib import Path
from subprocess import Popen
from typing import Optional

import web3
from eth_account.signers.local import LocalAccount
from web3 import Web3
from web3.eth import Eth
from web3.net import Net

from common.util.predicates import YES
from common.util.wait import wait

import os

_localhost = '127.0.0.1'


class Node:
    class InitMode:
        pass

    @dataclass
    class ManagedProcessInitMode(InitMode):
        executable_path: str
        clean_data = True

    @dataclass
    class RemoteInitMode(InitMode):
        host: str = _localhost

    def __init__(self, cfg_file_path, wallet_file_path, mode: InitMode, default_w3_provider_type='http'):
        wallet_file_path, cfg_file_path = Path(wallet_file_path), Path(cfg_file_path)
        with open(cfg_file_path, mode="r") as f:
            cfg = json.load(f)
        self._w3_by_type = {}
        self._proc = None
        with open(wallet_file_path, mode="r") as f:
            self.account: LocalAccount = web3.Account.from_key(json.load(f)["node_secret"])
        self.w3: Optional[Web3] = None
        self.eth: Optional[Eth] = None
        self.net: Optional[Net] = None

        atexit.register(self.destructor)

        net_host = _localhost
        if isinstance(mode, Node.ManagedProcessInitMode):
            if mode.clean_data:
                shutil.rmtree(Path(cfg["data_path"]), ignore_errors=True)
            os.makedirs(cfg["data_path"], exist_ok=True)

            datadir_cfg_file_path = Path(cfg["data_path"] + "/" + cfg_file_path.name)
            shutil.copyfile(cfg_file_path, datadir_cfg_file_path)

            datadir_wallet_file_path = Path(cfg["data_path"] + "/" + wallet_file_path.name)
            shutil.copyfile(wallet_file_path, datadir_wallet_file_path)

            # add default chain config section in config file and add default account to it
            Popen([mode.executable_path, "--command", "config", "--config", datadir_cfg_file_path, "--wallet", datadir_wallet_file_path, "--network-id", "0"]).wait()
            with open(datadir_cfg_file_path, mode="r") as f:
                cfg = json.load(f)
                cfg["chain_config"]["final_chain"]["state"]["genesis_balances"] = {}
                cfg["chain_config"]["final_chain"]["state"]["genesis_balances"]["de2b1203d72d3549ee2f733b00b2789414c7cea5"] = "0x1fffffffffffff"
                cfg["chain_config"]["final_chain"]["state"]["dpos"]["genesis_state"] = {}
                cfg["chain_config"]["final_chain"]["state"]["dpos"]["genesis_state"]["0xde2b1203d72d3549ee2f733b00b2789414c7cea5"] = {}
                cfg["chain_config"]["final_chain"]["state"]["dpos"]["genesis_state"]["0xde2b1203d72d3549ee2f733b00b2789414c7cea5"]["0xde2b1203d72d3549ee2f733b00b2789414c7cea5"] = "0x3b9aca00"
                cfg["chain_config"]["final_chain"]["state"]["execution_options"] = {}
                cfg["chain_config"]["final_chain"]["state"]["execution_options"]["disable_nonce_check"] = True
                cfg["chain_config"]["final_chain"]["state"]["execution_options"]["disable_gas_fee"] = True
            with open(datadir_cfg_file_path, mode="w") as f:
                json.dump(cfg, f)

            self._proc = Popen([mode.executable_path, "--config", datadir_cfg_file_path, "--wallet", datadir_wallet_file_path, "--data-dir", cfg["data_path"]])
        elif isinstance(mode, Node.RemoteInitMode):
            net_host = mode.host
        else:
            raise AssertionError("unknown init mode")

        cfg_rpc = cfg.get("rpc", {})
        rpc_http_port = cfg_rpc.get("http_port", None)
        if rpc_http_port is not None:
            self._w3_by_type['http'] = Web3(Web3.HTTPProvider(
                endpoint_uri=f"http://{net_host}:{rpc_http_port}",
                request_kwargs=dict(timeout=45),
            ))
        rpc_ws_port = cfg_rpc.get("ws_port", None)
        if rpc_ws_port is not None:
            self._w3_by_type['ws'] = Web3(Web3.WebsocketProvider(
                endpoint_uri=f"ws://{net_host}:{rpc_ws_port}",
                websocket_timeout=45,
            ))
        assert self._w3_by_type, "No API clients were created from the config - the node is barely testable"
        self.use_w3_provider(default_w3_provider_type)
        wait(lambda: self.net.listening,
             is_exception_ok=YES, fail_immediately=lambda _: self._proc and self._proc.poll() is None)

    @property
    def crashed(self):
        return self._proc and self._proc.poll() is not None

    # assumption: after this method is called the object is never used again
    def destructor(self):
        if self._proc:
            self._proc.terminate()

    def use_w3_provider(self, provider_type: str):
        w3 = self._w3_by_type.get(provider_type, None)
        assert w3 is not None, f"{provider_type} w3 provider is not available"
        self.w3, self.eth, self.net = w3, w3.eth, w3.net
        return self
