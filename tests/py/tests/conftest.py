import dataclasses
from pathlib import Path
from typing import List

import pytest

from common.node import terminate_all, Node
from common.test_helper import TestHelper

THIS_DIR = Path(__file__).parent


def pytest_addoption(parser):
    for field in dataclasses.fields(TestHelper):
        required = field.default == dataclasses.MISSING
        parser.addoption(f"--{field.name}",
                         action="store",
                         default=None if required else field.default,
                         required=required)


@pytest.fixture(scope="session")
def helper(pytestconfig):
    params = {}
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
