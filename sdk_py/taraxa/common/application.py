from abc import ABC, abstractmethod
from typing import *

from taraxa.common.io.io_driver import IODriver


class Application(ABC):

    def __init__(self, io_driver: IODriver):
        self.io_driver = io_driver

    @abstractmethod
    def exec(self, *args: Any) -> Any:
        pass
