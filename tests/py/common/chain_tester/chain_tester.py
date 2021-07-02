import traceback
from email.headerregistry import Address
from functools import cached_property
from typing import Optional, Union, List, Dict

from eth_account.signers.base import BaseAccount
from eth_account.signers.local import LocalAccount
from eth_typing import ChecksumAddress
from eth_utils import to_checksum_address, to_hex
from hexbytes import HexBytes
from web3.exceptions import BlockNotFound
from web3.types import TxParams, BlockIdentifier, TxData, TxReceipt

from common.chain_tester.misc import TransactionResultFuture, TransactionExpectation, \
    DefaultNonceStrategy, ContractTxExtraParams, CoinTxExtraParams
from common.eth.w3 import FilterTester, ContractFactory, ContractEvent
from common.node.cluster import Cluster
from common.util.asserts import assert_equal
from common.util.std_dataclasses import to_dict
from common.util.wait import wait, Timeout


# TODO check logs bloom
# TODO check account nonces, and contract address generation (similarly to how balances are tracked in this class)
# TODO check transaction and receipts root, eth block header hashing (use `eth_py`)
class ChainTester:
    NO_SIGNER: Optional[LocalAccount] = None

    class _PendingTransaction:

        def __init__(self, future: TransactionResultFuture, expectation: TransactionExpectation,
                     creation_params: TxParams):
            self.future, self.expectation = future, expectation
            # debug
            self._creation_params = creation_params
            tb = traceback.format_stack()
            self._creation_traceback_str = "".join(e for i, e in enumerate(tb) if i < len(tb) - 2)

        @cached_property
        def error_msg(self):
            return (f"Failed on validating the transaction with hash: {to_hex(self.future.hash)}\n"
                    f"params: {self._creation_params}\n"
                    f"and expectation: {self.expectation}.\n"
                    f"Originally sent at:\n{self._creation_traceback_str}")

    def __init__(self, cluster: Cluster,
                 assume_no_implicit_transfers=True,
                 auto_test_tx_and_blk_filters=False,
                 default_tx_signer=NO_SIGNER,
                 sync_timeout_per_blk_per_node=Timeout(num_attempts=200, backoff_seconds=1),
                 nonce_strategy=DefaultNonceStrategy()):
        self.default_tx_signer = default_tx_signer
        self.sync_timeout_per_blk_per_node = sync_timeout_per_blk_per_node

        self._cluster = cluster
        self._nonce_strategy = nonce_strategy
        self._pending_trxs: Dict[HexBytes, ChainTester._PendingTransaction] = {}
        self._executed_trxs: Dict[HexBytes, TxData] = {}
        self._receipts: Dict[HexBytes, TxReceipt] = {}
        self._contract_deployments_by_trx_hash: Dict[HexBytes, ChainTester.ContractDeployment] = {}
        self._track_balances = assume_no_implicit_transfers
        self._blocks_by_num = [self._get_block(0, full_transactions=True)]
        self._new_blk_filter_testers: List[FilterTester] = []
        self._new_transaction_filter_testers: List[FilterTester] = []
        if auto_test_tx_and_blk_filters:
            for node in self._cluster:
                self._new_blk_filter_testers.append(FilterTester(node.eth.filter('latest')))
                self._new_transaction_filter_testers.append(FilterTester(node.eth.filter('pending'),
                                                                         allow_duplicates=True))

    @property
    def last_blk_num(self):
        return len(self._blocks_by_num) - 1

    def sync(self):
        assert self._pending_trxs
        sync_results = []
        while self._pending_trxs:
            sync_results.append(self._sync_next_block())
        self._get_block(len(self._blocks_by_num), expect_absent=True)
        if self._track_balances:
            print("checking balances...")
        final_balances_to_check = {}
        for blk, balances_to_check in sync_results:
            for addr, val in balances_to_check.items():
                assert val == self._cluster.random_node.eth.get_balance(addr, block_identifier=blk.number)
                final_balances_to_check[addr] = val
        for addr, val in final_balances_to_check.items():
            assert val == self._cluster.random_node.eth.get_balance(addr)
        if self._new_blk_filter_testers:
            print("checking block filters...")
            synced_bk_hashes = {blk.hash for blk, _ in sync_results}
            for f in self._new_blk_filter_testers:
                f.test_poll(synced_bk_hashes)
        if self._new_transaction_filter_testers:
            print("checking tx filters...")
            synced_tx_hashes = {tx.hash for blk, _ in sync_results for tx in blk.transactions}
            for f in self._new_transaction_filter_testers:
                f.test_poll(synced_tx_hashes)
        print("sync done")

    def coin_transfer(self, to: Union[Address, ChecksumAddress, str], value: int,
                      tx_extra_params: Optional[CoinTxExtraParams] = None, node_index=None, signer=None,
                      expectation=TransactionExpectation()):
        assert to
        return self._send_tx(TxParams(to=to, value=value, **(to_dict(tx_extra_params) if tx_extra_params else dict())),
                             node_index=node_index, signer=signer, expectation=expectation)

    class ContractDeployment:

        def __init__(self, deployment_trx: TransactionResultFuture, chain: 'ChainTester',
                     contract_factory: ContractFactory):
            self.deployment_trx = deployment_trx
            self._chain = chain
            self._factory = contract_factory
            self._address = None

        @property
        def address(self):
            assert self._address is not None, "the contract is not yet deployed"
            return self._address

        @address.setter
        def address(self, val):
            assert self._address is None and val is not None
            self._address = val
            self._factory = self._factory.with_address(val)

        def call(self, method_name, *args,
                 tx_extra_params: Optional[ContractTxExtraParams] = None,
                 signer: Optional[BaseAccount] = None,
                 estimate_gas_only=False, block_id: BlockIdentifier = 'latest', node_index=None):
            assert method_name
            return self._chain._invoke_contract_method(self._factory, method_name, *args,
                                                       tx_extra_params=tx_extra_params,
                                                       dry_run=True, estimate_gas=estimate_gas_only,
                                                       dry_run_block_id=block_id,
                                                       signer=signer,
                                                       node_index=node_index)

        def execute(self, method_name, *args,
                    tx_extra_params: Optional[ContractTxExtraParams] = None,
                    signer: Optional[BaseAccount] = None,
                    node_index=None, expectation=TransactionExpectation()):
            assert method_name
            return self._chain._invoke_contract_method(self._factory, method_name, *args,
                                                       tx_extra_params=tx_extra_params,
                                                       node_index=node_index, signer=signer, expectation=expectation)

        def event(self, name, *constructor_arg_names_override, node_index=None):
            event_factory = getattr(self._factory(self._chain._cluster.node(node_index).eth).events, name)
            base = event_factory(*constructor_arg_names_override)
            return ContractEvent(base)

    def deploy_contract(self, contract_factory: ContractFactory, *args,
                        tx_extra_params: Optional[ContractTxExtraParams] = None,
                        signer: Optional[BaseAccount] = None,
                        node_index=None, expectation=TransactionExpectation()):
        pending_tx = self._invoke_contract_method(contract_factory, "", *args, tx_extra_params=tx_extra_params,
                                                  node_index=node_index, signer=signer, expectation=expectation)
        ret = ChainTester.ContractDeployment(pending_tx, self, contract_factory)
        self._contract_deployments_by_trx_hash[pending_tx.hash] = ret
        return ret

    def deploy_contract_estimate_gas(self, contract_factory: ContractFactory, *args,
                                     tx_extra_params: Optional[ContractTxExtraParams] = None,
                                     signer: Optional[BaseAccount] = None,
                                     node_index=None, block_id: BlockIdentifier = None):
        return self._invoke_contract_method(contract_factory, "", *args, tx_extra_params=tx_extra_params, signer=signer,
                                            dry_run=True, estimate_gas=True, dry_run_block_id=block_id,
                                            node_index=node_index)

    def _get_block(self, blk_identifier, expect_absent=False, full_transactions=False, **wait_kwargs):
        blocks = [wait(lambda: node.eth.get_block(blk_identifier, full_transactions=full_transactions),
                       is_value_ok=lambda v: not expect_absent,
                       is_exception_ok=lambda v: expect_absent and isinstance(v, BlockNotFound),
                       fail_immediately=lambda v: expect_absent and not isinstance(v, Exception),
                       **wait_kwargs)
                  for node in self._cluster]
        if not expect_absent:
            assert_equal(blocks)
            return blocks[0]

    def _sync_next_block(self):
        blk_n = len(self._blocks_by_num)
        print(f"syncing block {blk_n}...")
        blk = self._get_block(blk_n, full_transactions=True, timeout=self.sync_timeout_per_blk_per_node)
        blk_compact = self._get_block(blk_n)
        expanded_txs = list(blk.transactions)
        for tx_i, tx in enumerate(blk.transactions):
            blk.transactions[tx_i] = tx.hash
        assert blk_compact == blk
        for tx_i, tx in enumerate(expanded_txs):
            blk.transactions[tx_i] = tx
        assert blk == self._get_block(blk.hash, full_transactions=True)
        assert blk_compact == self._get_block(blk.hash)
        assert blk.parentHash == self._blocks_by_num[blk_n - 1].hash
        assert blk.number == blk_n
        assert blk.miner in {node.account.address for node in self._cluster}
        assert blk.miner == to_checksum_address(blk.author)
        assert blk.extraData == bytes()
        assert blk.gasLimit == 9007199254740991
        assert blk.mixHash == bytes(32)
        assert blk.nonce == bytes(8)
        assert blk.sha3Uncles == HexBytes('0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347')
        assert blk.uncles == []
        assert blk.totalDifficulty == blk.difficulty == 0
        assert_equal(len(blk.transactions),
                     *[node.eth.get_block_transaction_count(blk_n) for node in self._cluster])
        tracked_balances = {}
        expected_cumulative_gas_used = 0
        for tx_i, tx in enumerate(expanded_txs):
            pending_tx = self._pending_trxs.pop(tx.hash, None)
            assert pending_tx is not None, f"transaction {tx} is not known as pending"
            from_addr = getattr(tx, 'from')
            contract_deployment = self._contract_deployments_by_trx_hash.get(tx.hash, None)
            try:
                assert tx.hash not in self._executed_trxs
                assert_equal(tx,
                             *[node.eth.get_transaction(tx.hash) for node in self._cluster],
                             *[node.eth.get_transaction_by_block(blk_n, tx_i) for node in self._cluster])
                receipt = assert_equal(node.eth.get_transaction_receipt(tx.hash) for node in self._cluster)
                assert receipt.gasUsed <= tx.gas
                expected_cumulative_gas_used += receipt.gasUsed
                assert receipt.cumulativeGasUsed == expected_cumulative_gas_used
                assert receipt.status == (1 if pending_tx.expectation.is_ok else 0)
                assert 'root' not in receipt
                assert getattr(receipt, 'from') == from_addr
                assert receipt.to == tx.to
                assert receipt.transactionHash == tx.hash
                assert receipt.blockNumber == blk_n
                assert receipt.blockHash == blk.hash
                assert receipt.transactionIndex == tx_i
                if not tx.to:
                    assert receipt.contractAddress
                    assert contract_deployment
                    contract_deployment.address = receipt.contractAddress
                else:
                    assert not contract_deployment
                assert len(receipt.logs) == len(pending_tx.expectation.event_calls)
                event_logs = []
                for log_i, log in enumerate(receipt.logs):
                    assert log.blockHash == blk.hash
                    assert log.blockNumber == blk.number
                    assert log.logIndex == log_i
                    assert log.removed is False
                    assert log.transactionHash == tx.hash
                    assert log.transactionIndex == tx_i
                    expected_event_call = pending_tx.expectation.event_calls[log_i]
                    event_data = expected_event_call.event.process_log(log)
                    assert log.address == log.address == expected_event_call.event.address
                    assert event_data.args == expected_event_call.args
                    assert event_data.event == expected_event_call.event.name
                    event_logs.append(event_data)
                pending_tx.future.resolve(receipt, event_logs)
                self._receipts[tx.hash] = receipt
            except Exception as e:
                raise AssertionError(pending_tx.error_msg) from e
            self._executed_trxs[tx.hash] = tx
            if self._track_balances:
                for addr in [from_addr, tx.to]:
                    if addr and addr not in tracked_balances:
                        tracked_balances[addr] = self._cluster.node().eth.get_balance(addr, block_identifier=blk_n - 1)
                tracked_balances[from_addr] -= (tx.value + tx.gasPrice * tx.gas)
                if tx.to:
                    tracked_balances[tx.to] += tx.value
        self._blocks_by_num.append(blk)
        return blk, tracked_balances

    def _send_tx(self, tx: TxParams, node_index=None, signer: Optional[BaseAccount] = None,
                 expectation=TransactionExpectation()):
        signer = signer or self.default_tx_signer
        send_signed = signer is not None
        node = self._cluster.node(node_index)
        orig_params = tx
        tx = dict(**tx)
        if 'nonce' not in tx:
            if send_signed:
                tx['nonce'] = self._nonce_strategy(signer.address)
            else:
                if 'from' in tx:
                    tx['nonce'] = self._nonce_strategy(getattr(tx, 'from'))
                else:
                    tx['from'] = node.account.address
                    tx['nonce'] = self._nonce_strategy(node.account.address)
        if send_signed:
            if 'gasPrice' not in tx:
                tx['gasPrice'] = node.eth.gas_price
            if 'gas' not in tx:
                tx['gas'] = node.eth.estimate_gas(TxParams(**tx))
            tx_hash = node.eth.send_raw_transaction(signer.sign_transaction(TxParams(**tx)).rawTransaction)
        else:
            tx_hash = node.eth.send_transaction(TxParams(**tx))
        assert tx_hash not in self._pending_trxs and tx_hash not in self._executed_trxs
        ret = TransactionResultFuture(tx_hash)
        self._pending_trxs[tx_hash] = self._PendingTransaction(ret, expectation, orig_params)
        return ret

    def _invoke_contract_method(self, contract_factory, name, *args,
                                tx_extra_params: Optional[ContractTxExtraParams] = None,
                                signer: Optional[BaseAccount] = None,
                                dry_run=False, estimate_gas=False, dry_run_block_id=None,
                                node_index=None, expectation=TransactionExpectation()):
        tx_params = TxParams(**to_dict(tx_extra_params)) if tx_extra_params else None
        dry_run_block_id = 'latest' if dry_run_block_id is None else dry_run_block_id
        node = self._cluster.node(node_index)
        contract = contract_factory(node.eth)
        method = contract.constructor if not name else getattr(contract.functions, name)
        bound_method = method(*args)
        if dry_run:
            signer = signer or self.default_tx_signer
            if signer is not None:
                with_from = {'from': signer.address}
                tx_params = TxParams(**with_from, **tx_params) if tx_params else TxParams(**with_from)
            if estimate_gas:
                return bound_method.estimateGas(tx_params, dry_run_block_id)
            return bound_method.call(tx_params, dry_run_block_id)
        return self._send_tx(bound_method.buildTransaction(tx_params),
                             node_index=node_index, signer=signer, expectation=expectation)
