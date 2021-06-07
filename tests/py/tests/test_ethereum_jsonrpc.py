from common import solidity


# TODO [WIP] develop further, cover more stuff, extract patterns
def test_1(default_cluster):
    node_0 = default_cluster[0]
    contract_factory = solidity.compile_single_src('''
pragma solidity ^0.5.0;
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
    contract_deploy_trx_hash = contract_factory(node_0.w3_http.eth).constructor().transact()
    contract_deploy_receipt = default_cluster[1].w3_http.eth.wait_for_transaction_receipt(contract_deploy_trx_hash)
    assert contract_deploy_receipt.status == 1
    contract_0 = contract_factory(node_0.w3_http.eth, contract_deploy_receipt.contractAddress)
    greeting = "foo"
    set_greeting_trx_hash = contract_0.functions.setGreeting(greeting).transact()
    set_greeting_trx_receipt = default_cluster[2].w3_http.eth.wait_for_transaction_receipt(set_greeting_trx_hash)
    assert set_greeting_trx_receipt.status == 1
    print(contract_deploy_receipt)
    contract_3 = contract_factory(default_cluster[3].w3_http.eth, contract_deploy_receipt.contractAddress)
    assert contract_3.functions.greet().call() == greeting
