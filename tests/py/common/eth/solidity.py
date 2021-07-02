import json

import solcx

from common.eth.w3 import ContractFactory


def compile_single(src: str):
    solc_result = solcx.compile_standard({
        "language": "Solidity",
        "sources": {
            "arbitrary_string": {
                "content": src
            }
        },
        "settings":
            {
                "outputSelection": {
                    "*": {
                        "*": [
                            "metadata",
                            "evm.bytecode",
                            "evm.bytecode.sourceMap"
                        ]
                    }
                }
            }
    })
    contract_bin = next(iter(solc_result["contracts"]["arbitrary_string"].values()))
    contract_metadata = json.loads(contract_bin["metadata"])
    return ContractFactory(bytecode=contract_bin["evm"]["bytecode"]["object"], abi=contract_metadata["output"]["abi"])
