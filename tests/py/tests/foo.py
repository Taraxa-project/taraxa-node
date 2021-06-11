#
# from jsonrpcclient.clients.http_client import HTTPClient
# from jsonrpcclient.requests import Request
# from web3 import Web3, HTTPProvider
#
#
#
# def get_staking_bal(blk, addr):
#     return int(
#         request(endpoint, "taraxa_queryDPOS", {
#             "block_number": hex(blk),
#             "with_eligible_count": True,
#             "account_queries": {
#                 addr: {'with_staking_balance': True},
#             },
#         }).data.result['account_results'][addr]['staking_balance'],
#         16
#     )
import json
from pathlib import Path

from jsonrpcclient.clients.http_client import HTTPClient
from jsonrpcclient.requests import Request
from web3 import HTTPProvider, Web3

endpoint = "https://rpc.testnet.taraxa.io"
output_file = Path("/home/oleg/projects/github.com/Taraxa-project/taraxa-node-3/tests/py/foo.json")
blk_n = 43026
batch_size = 100

jsonrpc_client = HTTPClient(endpoint)
w3 = Web3(HTTPProvider(endpoint))
tx_hashes = w3.eth.get_block(blk_n).transactions

# output_file.unlink(missing_ok=True)
print(f"num transactions: {len(tx_hashes)}")
for batch_i in range(213, len(tx_hashes) // batch_size):
    begin = batch_i * batch_size
    end = min(len(tx_hashes), begin + batch_size)
    print(f"processing transactions # [{begin}; {end})")
    resp = jsonrpc_client.send(
        [Request("eth_getTransactionByHash", tx_hashes[tx_i].hex()) for tx_i in range(begin, end)])
    for resp_i in resp.data:
        assert resp_i.ok
        tx = resp_i.result
        if tx.get('to', None) == "0x00000000000000000000000000000000000000ff":
            s = json.dumps(tx)
            print(f"HIT: \n{s}")
            with open(output_file, mode='a') as f:
                f.write(s)
                f.write("\n")
