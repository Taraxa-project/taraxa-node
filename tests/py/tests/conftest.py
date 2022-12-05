import dataclasses
from pathlib import Path

import pytest

from common.node.cluster import Cluster
from common.node.node import Node
from common.test_config import TestConfig

THIS_DIR = Path(__file__).parent


def pytest_addoption(parser):
    for field in dataclasses.fields(TestConfig):
        required = field.default == dataclasses.MISSING
        parser.addoption(f"--{field.name}",
                         action="store",
                         default=None if required else field.default,
                         required=required)


@pytest.fixture(scope="session")
def test_cfg(pytestconfig):
    params = {}
    for field in dataclasses.fields(TestConfig):
        val = pytestconfig.getoption(field.name)
        if val is not None:
            params[field.name] = val
    return TestConfig(**params)


@pytest.fixture(scope="session")
def default_node_configs():
    basedir = THIS_DIR.parent.parent.joinpath("util_test").joinpath("conf")
    return [(basedir.joinpath(f"conf_taraxa{i + 1}.json"), basedir.joinpath(f"wallet{i + 1}.json"), basedir.joinpath(f"genesis{i + 1}.json")) for i in range(5)]


@pytest.fixture()
def default_cluster(test_cfg, default_node_configs) -> Cluster:
    cluster = Cluster.from_new_nodes(default_node_configs,
                                     Node.ManagedProcessInitMode(executable_path=test_cfg.node_executable_path))
    yield cluster
    cluster.destructor()


@pytest.fixture()
def default_cluster_unmanaged(default_node_configs) -> Cluster:
    cluster = Cluster.from_new_nodes(default_node_configs, Node.RemoteInitMode())
    yield cluster
    cluster.destructor()
