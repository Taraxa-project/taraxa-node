from eth.db.trie import make_trie_root_and_nodes
from eth.vm.forks.berlin.transactions import BerlinTransactionBuilder
from web3.types import TxData, TxReceipt


# This was supposed to be a utility package that helps converting ethereum data structures between `web3` and `eth_py`.
# `eth_py` is a python EVM implementation which also contains python classes for every type we need in our scope.
# `eth_py` provides something that web3 doesn't - utilities to derive ethereum log blooms, construct tries,
# and in particular to derive receipts and transactions roots for the ethereum block header.
# TODO use `eth_py` to derive the binary representation of ethereum block headers in Taraxa, and compare that
# to what we return from our ethereum json-rpc implementation


def map_web3_tx_signed(tx: TxData):
    return BerlinTransactionBuilder.new_transaction(
        tx.nonce,
        tx.gasPrice,
        tx.gas,
        tx.to,
        tx.value,
        tx.data,
        tx.v,
        tx.r,
        tx.s,
    )


def map_web3_receipt(r: TxReceipt):
    pass
    # return Receipt()


def trie_root(txs_or_receipts):
    return make_trie_root_and_nodes(txs_or_receipts)[0]
