from jsonrpcclient import request
from jsonrpcclient.clients.http_client import HTTPClient
from web3 import HTTPProvider, Web3

endpoint = "https://rpc.testnet.taraxa.io"

blk_n = 43026
jsonrpc_client = HTTPClient(endpoint)
w3 = Web3(HTTPProvider(endpoint))
pass