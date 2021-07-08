from typing import Iterable

import web3.contract
from web3._utils.filters import Filter
from web3.eth import Eth

from common.util.predicates import SetHasBeenMatched
from common.util.wait import Timeout, wait


class FilterTester:

    def __init__(self, _filter: Filter, allow_duplicates=False, collect_polled=False):
        self._filter = _filter
        self._allow_duplicates = allow_duplicates
        self._polled_so_far = [] if collect_polled else None

    def test_poll(self, expected_entries: Iterable, timeout=Timeout()):
        collect = self._polled_so_far is not None
        wait_condition = SetHasBeenMatched(expected_entries, allow_duplicates=self._allow_duplicates, collect=collect)
        wait(self._filter.get_new_entries, is_value_ok=wait_condition, timeout=timeout)
        if collect:
            self._polled_so_far.extend(wait_condition.collected)
        return self

    def test_get_filter_by_id(self):
        same_filter = self._filter.eth_module.filter(filter_id=self._filter.filter_id)
        assert same_filter.filter_id == self._filter.filter_id
        return self

    def test_get_all_entries(self):
        assert self._polled_so_far is not None, "collection of polled entries was not enabled for this instance"
        assert self._filter.get_all_entries() == self._polled_so_far
        return self

    # after this it is not usable
    def test_uninstall(self):
        self._filter.eth_module.uninstall_filter(self._filter.filter_id)
        assert len(self._filter.get_all_entries()) == len(self._filter.get_new_entries()) == 0


class ContractFactory:

    def __init__(self, deployed_address=None, **kwargs):
        self._deployed_address = deployed_address
        self._kwargs = kwargs

    def with_address(self, deployed_address):
        return ContractFactory(deployed_address=deployed_address, **self._kwargs)

    def __call__(self, eth: Eth):
        return eth.contract(address=self._deployed_address, **self._kwargs)


class ContractEvent:
    class Called:
        def __init__(self, event: 'ContractEvent', **constructor_args):
            self.event, self.args = event, constructor_args

        def create_filter(self, **kwargs):
            return self.event.filter_tester(**kwargs, **self.args)

    def __init__(self, base: web3.contract.ContractEvent):
        self._base = base

    @property
    def address(self):
        return self._base.address

    @property
    def name(self):
        return self._base.event_name

    @property
    def arg_names(self):
        return self._base.argument_names

    def called(self, **constructor_args):
        return self.Called(self, **constructor_args)

    def filter_tester(self, from_block='latest', to_block='latest', enable_all_entries_test=True, **constructor_args):
        f = self._base.createFilter(fromBlock=from_block, toBlock=to_block,
                                    argument_filters=None if not constructor_args else constructor_args)
        return FilterTester(f, collect_polled=enable_all_entries_test)

    def process_receipt(self, *args, **kwargs):
        return self._base.processReceipt(*args, **kwargs)

    def process_log(self, *args, **kwargs):
        return self._base.processLog(*args, **kwargs)
