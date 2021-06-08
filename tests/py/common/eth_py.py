from eth.db.trie import make_trie_root_and_nodes
from eth.rlp.receipts import Receipt
from eth.vm.forks.berlin.transactions import BerlinTransactionBuilder
from web3.types import TxData, TxReceipt


# # syntax b/c "from" keyword not allowed w/ class construction
# TxData = TypedDict("TxData", {
#     "blockHash": HexBytes,
#     "blockNumber": BlockNumber,
#     "chainId": int,
#     "data": Union[bytes, HexStr],
#     "from": ChecksumAddress,
#     "gas": Wei,
#     "gasPrice": Wei,
#     "hash": HexBytes,
#     "input": HexStr,
#     "nonce": Nonce,
#     "r": HexBytes,
#     "s": HexBytes,
#     "to": ChecksumAddress,
#     "transactionIndex": int,
#     "v": int,
#     "value": Wei,
# }, total=False)

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

# TxReceipt = TypedDict("TxReceipt", {
#     "blockHash": HexBytes,
#     "blockNumber": BlockNumber,
#     "contractAddress": Optional[ChecksumAddress],
#     "cumulativeGasUsed": int,
#     "gasUsed": Wei,
#     "from": ChecksumAddress,
#     "logs": List[LogReceipt],
#     "logsBloom": HexBytes,
#     "root": HexStr,
#     "status": int,
#     "to": ChecksumAddress,
#     "transactionHash": HexBytes,
#     "transactionIndex": int,
# })

def map_web3_receipt(r: TxReceipt):
    return Receipt(

    )


def trie_root(txs_or_receipts):
    return make_trie_root_and_nodes(txs_or_receipts)[0]

#     @classmethod
#     def new_transaction(
#             cls,
#             nonce: int,
#             gas_price: int,
#             gas: int,
#             to: Address,
#             value: int,
#             data: bytes,
#             v: int,
#             r: int,
#             s: int) -> SignedTransactionAPI:
#         return cls.legacy_signed(nonce, gas_price, gas, to, value, data, v, r, s)
