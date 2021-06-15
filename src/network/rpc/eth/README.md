### This package contains the implementation of Ethereum standard JSON-RPC API.

The following resources were used as the source of truth:

- https://eth.wiki/json-rpc/API
- https://ethereum.org/en/developers/docs/apis/json-rpc/

# Quirks

- "pending" block identifier means "latest"
- `eth_syncing` return values are to be better defined

# Not implemented

All uniplemented methods will return a standard json-rpc error that the method is not present

#### Simple to implement, should implement:

- web3_clientVersion
- web3_sha3

#### PoW methods, probably no need to implement, although we could try returning some sensible default/null values:

- eth_mining
- eth_hashrate
- eth_getWork
- eth_submitWork
- eth_submitHashrate

#### Contract compiling methods. Require compiler dependencies in the runtime, simple to implement, questionable usefulness:

- eth_getCompilers
- eth_compileSolidity
- eth_compileLLL
- eth_compileSerpent

#### Arbitrary K/V database methods. Deprecated in the spec, simple to implement, questionable usefulness:

- db_putString
- db_getString
- db_putHex
- db_getHex

#### These correspond to the Whisper Ethereum protocol, which we never even thought about:

- shh_version
- shh_post
- shh_newIdentity
- shh_hasIdentity
- shh_newGroup
- shh_addToGroup
- shh_newFilter
- shh_uninstallFilter
- shh_getFilterChanges
- shh_getMessages