# TODO [WIP] develop further, cover more stuff, extract patterns

from typing import List

from eth_utils import to_checksum_address
from hexbytes import HexBytes
from web3.exceptions import BlockNotFound
from web3.types import TxData

from common import solidity, asserts
from common.node import Node
from common.wait import wait


def test_1(default_cluster):
    return  # TODO
    cluster = default_cluster
    contract_factory = solidity.compile_single_src('''
pragma solidity ^0.8.0;
contract Greeter {
    string public greeting;
    
    constructor() public {
       greeting = 'Hello';
    }
    
    function setGreeting(string memory _greeting) public {
       greeting = _greeting;
    }
    
    function greet() view public returns (string memory) {
       return greeting;
    }
}
''')
    contract_factory(cluster[0].w3_http.eth).constructor()
    contract_deploy_trx_hash = contract_factory(cluster[0].w3_http.eth).constructor().transact()
    contract_deploy_receipt = cluster[1].w3_http.eth.wait_for_transaction_receipt(contract_deploy_trx_hash)
    assert contract_deploy_receipt.status == 1

    check_block(cluster, 5, expect_absent=True)
    check_block(cluster, 1)

    contract_0 = contract_factory(cluster[0].w3_http.eth, contract_deploy_receipt.contractAddress)
    greeting = "foo"
    set_greeting_trx_hash = contract_0.functions.setGreeting(greeting).transact()
    set_greeting_trx_receipt = cluster[2].w3_http.eth.wait_for_transaction_receipt(set_greeting_trx_hash)
    assert set_greeting_trx_receipt.status == 1
    contract_3 = contract_factory(cluster[2].w3_http.eth, contract_deploy_receipt.contractAddress)
    assert contract_3.functions.greet().call() == greeting


def get_block(nodes: List[Node], blk_identifier, expect_absent=False, **eth_get_block_kwargs):
    assert 0 < len(nodes)
    blocks = [wait(lambda: n.w3_http.eth.get_block(blk_identifier, **eth_get_block_kwargs),
                   is_value_ok=lambda v: not expect_absent,
                   is_exception_ok=lambda v: expect_absent and isinstance(v, BlockNotFound),
                   fail_immediately=lambda v: expect_absent and not isinstance(v, Exception))
              for n in nodes]
    if expect_absent:
        return
    asserts.EQ(blocks)
    return blocks[0]


def compare_compact_and_expanded_block(compact, expanded):
    expanded_txs = list(expanded.transactions)
    for i, tx in enumerate(expanded.transactions):
        expanded.transactions[i] = tx.hash
    assert compact == expanded
    for i, tx in enumerate(expanded_txs):
        expanded.transactions[i] = tx


def check_block(nodes: List[Node], blk_n: int, expect_absent=False,
                expected_transactions: [List[TxData]] = tuple()):
    assert 0 <= blk_n
    blk_compact = get_block(nodes, blk_n, expect_absent=expect_absent)
    blk = get_block(nodes, blk_n, expect_absent=expect_absent, full_transactions=True)
    if expect_absent:
        return
    compare_compact_and_expanded_block(blk_compact, blk)
    assert blk == get_block(nodes, blk.hash, full_transactions=True)
    assert blk_compact == get_block(nodes, blk.hash)
    if blk_n:
        assert blk.parentHash == get_block(nodes, blk_n - 1).hash
    assert blk.number == blk_n
    assert blk.miner in {node.account.address for node in nodes}
    assert blk.miner == to_checksum_address(blk.author)  # TODO is it ok?
    assert blk.extraData == bytes()
    assert blk.gasLimit == 9007199254740991
    assert blk.mixHash == bytes(32)
    assert blk.nonce == bytes(8)
    assert blk.sha3Uncles == HexBytes('0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347')
    assert blk.uncles == []
    assert blk.totalDifficulty == blk.difficulty == 0
    assert blk.transactions == expected_transactions
    transaction_counts_by_blk_n = [node.w3_http.eth.getTransactionByBlock(blk_n) for node in nodes]
    asserts.EQ(transaction_counts_by_blk_n)
    assert transaction_counts_by_blk_n[0] == len(expected_transactions)
    assert transaction_counts_by_blk_n == [node.w3_http.eth.getTransactionByBlock(blk.hash) for node in nodes]
