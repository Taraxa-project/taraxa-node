from typing import *

from taraxa.common.application import Application


class Keygen(Application):
    class Result(NamedTuple):
        secret: str
        pubkey: str
        address: str

    def exec(self) -> Result:
        process = self.io_driver.call("keygen")
        return Keygen.Result(*(process.stdout.readline()[:-1] for _ in range(3)))
