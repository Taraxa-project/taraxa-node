from dataclasses import dataclass


@dataclass(frozen=True)
class TestConfig:
    node_executable_path: str
