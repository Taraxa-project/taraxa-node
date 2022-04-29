from dataclasses import dataclass
from typing import Optional, List

from web3.types import Wei, Nonce, EventData, TxReceipt

from common.eth.w3 import ContractEvent


class TransactionExpectation:
    def __init__(self, *event_calls: ContractEvent.Called, is_ok=True, gas_used: Optional[Wei] = None):
        self.is_ok, self.gas_used, self.event_calls = is_ok, gas_used, event_calls

    def __str__(self):
        return str(dict(is_ok=self.is_ok, gas_used=self.gas_used, event_calls=self.event_calls))


class TransactionResultFuture:

    def __init__(self, tx_hash):
        self._tx_hash = tx_hash
        self._ready, self._receipt, self._event_logs = False, None, None

    @property
    def hash(self):
        return self._tx_hash

    @property
    def receipt(self) -> TxReceipt:
        assert self._ready
        return self._receipt

    @property
    def event_logs(self) -> List[EventData]:
        assert self._ready
        return self._event_logs

    def resolve(self, receipt: TxReceipt, event_logs: List[EventData]):
        assert not self._ready
        self._ready, self._receipt, self._event_logs = True, receipt, event_logs


@dataclass
class ContractTxExtraParams:
    chainId: Optional[int] = None
    gas: Optional[Wei] = None
    gasPrice: Optional[Wei] = None
    nonce: Optional[Nonce] = None
    value: Optional[Wei] = None


@dataclass
class CoinTxExtraParams:
    chainId: Optional[int] = None
    gas: Optional[Wei] = None,
    gasPrice: Optional[Wei] = None,
    nonce: Optional[Nonce] = None,


class DefaultNonceStrategy:

    def __init__(self):
        self.i = 0

    def __call__(self, addr):
        prev, self.i = self.i, self.i + 1
        return prev

    def update(self, new_value):
        self.i = new_value