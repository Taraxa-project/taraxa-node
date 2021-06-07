import dataclasses
import json
import os
from pathlib import Path
from typing import List

import pytest

from common.node import terminate_all, Node
from common.test_helper import TestHelper

THIS_DIR = Path(__file__).parent

local_cfg_file_path = Path(os.getcwd()).joinpath("taraxa_node_tests_py_config.json")
config_from_file = local_cfg_file_path.exists()
if config_from_file:
    print(f"configuring the tests from file: {local_cfg_file_path}")


def pytest_addoption(parser):
    if config_from_file:
        return
    for field in dataclasses.fields(TestHelper):
        required = field.default == dataclasses.MISSING
        parser.addoption(f"--{field.name}",
                         action="store",
                         default=None if required else field.default,
                         required=required)


@pytest.fixture(scope="session")
def helper(pytestconfig):
    params = {}
    if config_from_file:
        with open(local_cfg_file_path, mode='r') as f:
            params = json.load(f)
    else:
        for field in dataclasses.fields(TestHelper):
            val = pytestconfig.getoption(field.name)
            if val is not None:
                params[field.name] = val
    return TestHelper(**params)


@pytest.fixture(scope="session")
def default_node_configs():
    basedir = THIS_DIR.parent.parent.joinpath("util_test").joinpath("conf")
    return [basedir.joinpath(f"conf_taraxa{i + 1}.json") for i in range(5)]


@pytest.fixture()
def default_cluster(helper, default_node_configs) -> List[Node]:
    nodes = helper.start_nodes(*default_node_configs)
    yield nodes
    terminate_all(*nodes)
