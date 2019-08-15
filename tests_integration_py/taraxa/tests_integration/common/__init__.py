from web3.eth import Account
from eth_keys.datatypes import PrivateKey
from typing import *
import taraxa.tests_integration.common.paths
import json
from pathlib import Path


def random_private_key():
    return PrivateKey(Account.create().key)


def make_node_cfg_file(pk: PrivateKey = None, base_config: dict = None) -> Tuple[Path, dict]:
    pk = pk or random_private_key()
    base_config = base_config or {}
    node_workspace = paths.tmpdir.joinpath(f'node_{pk}')
    config = dict(node_secret=str(pk),
                  db_path=str(node_workspace.joinpath('db')),
                  **base_config)
    node_workspace.mkdir(parents=True, exist_ok=True)
    with node_workspace.joinpath('config.json').open(mode='w') as conf_file:
        json.dump(config, conf_file)
    return Path(conf_file.name), config
