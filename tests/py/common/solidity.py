import json

import solcx


def compile_single_src(src: str):
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
    contract_bin = solc_result["contracts"]["arbitrary_string"]["Greeter"]
    contract_metadata = json.loads(contract_bin["metadata"])
    return lambda eth, deployed_address=None: eth.contract(address=deployed_address,
                                                           bytecode=contract_bin["evm"]["bytecode"]["object"],
                                                           abi=contract_metadata["output"]["abi"])
