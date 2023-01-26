# Taraxa RPC

## Ethereum compatibility

### Quirks

- "pending" block identifier means "latest"
- `eth_syncing` return values are to be better defined

### Not implemented

Taraxa protocol supports all Ethereum methods described on [this page](https://eth.wiki/json-rpc/API#json-rpc-methods) except:

- web3_clientVersion
- web3_sha3
- eth_mining
- eth_hashrate
- eth_getWork
- eth_submitWork
- eth_submitHashrate
- eth_getCompilers
- eth_compileSolidity
- eth_compileLLL
- eth_compileSerpent
- db_putString
- db_getString
- db_putHex
- db_getHex
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

All unimplemented methods will return a standard json-rpc error that the method is not present

## Taraxa specific methods

### taraxa_protocolVersion

Returns current taraxa network protocol version

#### Parameters

none

#### Returns

`QUANTITY` - The current network protocol version

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_protocolVersion","params":[],"id":1}'

// Result
{
  "id":67,
  "jsonrpc":"2.0",
  "result": "0x2"
}
```

### taraxa_getVersion

Returns extended description of Taraxa node version.

#### Parameters

none

#### Returns

`OBJECT`, An object with info about current version
* `version`: `STRING` - Triplet describing full version of node in format `MAJOR.MINOR.PATCH`
* `git_branch`: `STRING` - Name of the branch node was built from
* `git_commit_hash`: `DATA`, 20 Bytes - Hash of the commit node was built from
* `git_commit_date`: `STRING` - Date of the commit node was built from
* `git_description`: `STRING` - Git description of commit node was built from

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_getVersion","params":[],"id":1}'

// Result
{
  "id":67,
  "jsonrpc":"2.0",
  "result": {
    "version": "1.0.2",
    "git_branch": "develop",
    "git_commit_hash": "0e2abae5cec9159702fbe6521005f901c5c24ed8",
    "git_commit_date": "Mon, 23 May 2022 16:53:09 +0200",
    "git_description": "v1.0.1-candidate-405-g0e2abae5"
  }
}
```

### taraxa_getDagBlockByHash

Returns information about DAG block by hash

#### Parameters

1. `DATA`, 32 Bytes - Hash of a block.
2. `Boolean` - If true it returns the full transaction objects, if false only the hashes of the transactions.

#### Returns

`OBJECT` - A DAG block object, or null when no block was found:

* `pivot`: `DATA`, 32 Bytes -
* `level`: `QUANTITY` - Level at which block was produced
* `period`: `QUANTITY` - Finalization period of this block
* `tips`: `ARRAY` - List of tips
* `transactions`: `ARRAY` - Array of transaction objects, or 32 Bytes transaction hashes depending on the last given parameter.
* `trx_estimations`: `ARRAY` - Array of `QUANTITIES` that means transaction gas estimation for corresponding transaction. For example `trx_estimations[0]` is for `transactions[0]`
* `sig`: `DATA`, 65 Bytes - Signature of block creator
* `hash`: `DATA`, 32 Bytes - Hash of this block
* `sender`: `DATA`, 20 Bytes - Address of block creator
* `timestamp`: `QUANTITY` - Time of block creation
* `vdf`: `OBJECT` - vdf proof for this block
  * `pk`: `DATA`, 32 Bytes - Public key
  * `proof`: `DATA`, 80 Bytes - Bytes of proof
  * `sol1`: `DATA` - First part of solution
  * `sol1`: `DATA` - Second part of solution
  * `difficulty`: `QUANTITY` - Difficulty with which block was produced

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_getDagBlockByHash","params": ["0x73fabc5e2ff3863b8c8a313ee0200b915ef2f3606d24f3ff817a4655c04852a6", false],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "hash": "0x73fabc5e2ff3863b8c8a313ee0200b915ef2f3606d24f3ff817a4655c04852a6",
    "level": "0xea1c6a",
    "period": "0x1d523f",
    "pivot": "0x7611f8f12ec5ad55bc86ea01b81cc02e42972c0142ff78a7998265428b1a44e7",
    "sender": "0xd4e4728bea5a67dd70dccb742bdc9c3a48465bec",
    "sig": "0xe0ca2ea180b90fa605f62334780893a282991dfda9cfb8d376a122fdd76878c2640e5ab95008082d30c99dea39bcb22dc4fc41ea66d9cfca3bc3e06f44f22bb701",
    "timestamp": "0x628f5829",
    "tips": [
      "0x8f267d6cd3d312c39739f55e078a9c3687edd26404bdb1f14bf0dd2ec6a093e3"
    ],
    "transactions": [
      "0xa0fe8112059edd24ce6486b429ef3e17e5338aa395d762f36b2f9271d8c1fce9", ...
    ],
    "trx_estimations": [
      22000, ...
    ]
      "vdf": {
      "difficulty": "0x11",
      "pk": "0x3258951e738ca5322b25f929f2c4bbbd1ebcef9acbf306a38ab9f1a5264af0eb",
      "proof": "0x5af4ebd0274f67d759ef5c87b90351d0c1cd5fb017a2b39387423973dd4126e916e44ee472b04876c9520b6f2d81be6dfbf1395c62a8412f4074e578791225fff52d387afb269176600f1daa8699b405",
      "sol1": "0x1c4ec46437cc82bc358014096d0730224a232c7554c12481fdf6c99480b92d632def11d57ac3e51b26b2a30a4e376b49661ddf91d11f3d2870cf172c15cd66df51c65464f875da1638825873192a7d830dbce6d5d1ca9e20a33b784a39d162e79d3c4061b9e6c5078323b5b536b4fdba84087f8159fdc08ca27779a0e66c964cbeeadae1dfcf8c8c1e525fdb283438fd3f9c4e1ae39818f280e8c7fd8c069c6a54c27dcddd1e6758b53d54c3973416a03ff3dab941a96e098ef963b3a763a9bb37c4658a4b9695e275ec543e79a825236e27a577d9ce779f831e68b0195b3f8f6f8993cae7dd6faaac7241c6c64580567991ca442a40e7093a3457c589a34a32",
      "sol2": "0x220effaba1e781cd9e3302416e821a4c24750f1150320cac207805cf78accec4ce18fe2dd9d0bba38009fdade0e9e31c8478aec1bdff782768f52f65cd42260fbb3ee572c5d3784e4ff1b1d57ab53383a5c9d0c6ee17c6c06af788c8d51e651184d7b9ee7cc972cf5dab14f0c45fe3fcdcf25465a531abd4e8cef1407177225a4dd28b404ab6f1772ee8d16ac4c28ef700a22f351d4afa601068177dd48f4d5a7660ff6a5c5aa4ce32551ad78f846054946262b190168bed0aa871e69b704bdd74b6fcb788acc7b410af6f6bbe09ff562d42cc6b2f6ebf3e51a52e8d263d6be3f3d1447d31f7b09cdeab384136d86f4910db8c99426b6fe5ed1f885c6c0aedb0"
    }
  }
}
```

### taraxa_getDagBlockByLevel

Returns information about DAG blocks for specified level

#### Parameters

1. `QUANTITY` - Level to get blocks from
2. `Boolean` - If true it returns the full transaction objects, if false only the hashes of the transactions.

#### Returns

`ARRAY` of `OBJECT` - A DAG block object, or null when no block was found:
* `pivot`: `DATA`, 32 Bytes -
* `level`: `QUANTITY` - Level at which block was produced
* `period`: `QUANTITY` - Finalization period of this block
* `tips`: `ARRAY` - List of tips
* `transactions`: `ARRAY` - Array of transaction objects, or 32 Bytes transaction hashes depending on the last given parameter.
* `trx_estimations`: `ARRAY` - Array of `QUANTITIES` that means transaction gas estimation for corresponding transaction. For example `trx_estimations[0]` is for `transactions[0]`
* `sig`: `DATA`, 65 Bytes - Signature of block creator
* `hash`: `DATA`, 32 Bytes - Hash of this block
* `sender`: `DATA`, 20 Bytes - Address of block creator
* `timestamp`: `QUANTITY` - Time of block creation
* `vdf`: `OBJECT` - vdf proof for this block
  * `pk`: `DATA`, 32 Bytes - Public key
  * `proof`: `DATA`, 80 Bytes - Bytes of proof
  * `sol1`: `DATA` - First part of solution
  * `sol1`: `DATA` - Second part of solution
  * `difficulty`: `QUANTITY` - Difficulty with which block was produced

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_getDagBlockByLevel","params":["0xEA1C6A", false],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": [
    {
      "hash": "0x038ab6a764130199849cd79916d0f310f300edda22c547759ff063839fc574c0",
      "level": "0xea1c6a",
      "period": "0x1d523f",
      "pivot": "0x7611f8f12ec5ad55bc86ea01b81cc02e42972c0142ff78a7998265428b1a44e7",
      "sender": "0x30273be3d2bc695f861e9319ec8197b39056a0c5",
      "sig": "0xddf6ac39b9465b897af5c852c90c2e5ec25c08114ca2a4e1bf582cb065d0e6237e4df1016143c8a7a3efe638240c50ae55e55a8701e01cc58fac566405736a1401",
      "timestamp": "0x628f5828",
      "tips": [],
      "transactions": [
        "0x35b2e032eb6b248099e4afb03297f0ef628cc2e768c0b611169b2999125aac88", ...
      ],
      "trx_estimations": [
        22000, ...
      ],
      "vdf": {
        "difficulty": "0x11",
        "pk": "0x13d9157caaefe6ce5795fdbed6b3994ed83a3fa0dae4384575aa259eeb658321",
        "proof": "0xffe0c378dbf2549cb9fe5f36ab645cd64e9231e801163c7d052fd5831148f5cfa93dc7e670d9f974825a13a2be8fe3ed6edca275eff93fb84556b960a67d2eb83ab0dd1f7f92bac3c174d9173467f905",
        "sol1": "0x1c4ec46437cc82bc358014096d0730224a232c7554c12481fdf6c99480b92d632def11d57ac3e51b26b2a30a4e376b49661ddf91d11f3d2870cf172c15cd66df51c65464f875da1638825873192a7d830dbce6d5d1ca9e20a33b784a39d162e79d3c4061b9e6c5078323b5b536b4fdba84087f8159fdc08ca27779a0e66c964cbeeadae1dfcf8c8c1e525fdb283438fd3f9c4e1ae39818f280e8c7fd8c069c6a54c27dcddd1e6758b53d54c3973416a03ff3dab941a96e098ef963b3a763a9bb37c4658a4b9695e275ec543e79a825236e27a577d9ce779f831e68b0195b3f8f6f8993cae7dd6faaac7241c6c64580567991ca442a40e7093a3457c589a34a32",
        "sol2": "0x220effaba1e781cd9e3302416e821a4c24750f1150320cac207805cf78accec4ce18fe2dd9d0bba38009fdade0e9e31c8478aec1bdff782768f52f65cd42260fbb3ee572c5d3784e4ff1b1d57ab53383a5c9d0c6ee17c6c06af788c8d51e651184d7b9ee7cc972cf5dab14f0c45fe3fcdcf25465a531abd4e8cef1407177225a4dd28b404ab6f1772ee8d16ac4c28ef700a22f351d4afa601068177dd48f4d5a7660ff6a5c5aa4ce32551ad78f846054946262b190168bed0aa871e69b704bdd74b6fcb788acc7b410af6f6bbe09ff562d42cc6b2f6ebf3e51a52e8d263d6be3f3d1447d31f7b09cdeab384136d86f4910db8c99426b6fe5ed1f885c6c0aedb0"
      }
    }, ...
  ]
}
```

### taraxa_dagBlockLevel

Returns current maximum level of DAG

#### Parameters

none

#### Returns

`QUANTITY` - The current maximum DAG level

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_dagBlockLevel","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": "0xea1f7f"
}
```

### taraxa_dagBlockPeriod

Returns last period

#### Parameters

none

#### Returns

`String` - The current protocol version

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_dagBlockPeriod","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": "0x1d52a3"
}
```

### taraxa_getScheduleBlockByPeriod

Returns PBFT block info with DAG block schedule for period

#### Parameters

`QUANTITY` - period

#### Returns

`OBJECT` - PBFT block info
* `beneficiary`: `DATA`, 20 Bytes - Address of beneficiary(block creator)
* `block_hash`: `DATA`, 32 Bytes - Hash of the block
* `dag_block_hash_as_pivot`: `DATA`, 32 Bytes - Hash of the pivot block
* `order_hash`: `DATA`, 32 Bytes - Hash of DAG blocks order
* `period`: `QUANTITY` - Period number
* `prev_block_hash`: `DATA`, 32 Bytes - Hash of previous PBFT block
* `schedule`: `OBJECT`
  * `dag_blocks_order`: `ARRAY` - DAG blocks order for current PBFT block
* `signature`: `DATA`, 65 Bytes - Beneficiary signature of the block
* `timestamp`: `QUANTITY` - Time of block generation

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_getScheduleBlockByPeriod","params":["0x1D533B"],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "beneficiary": "fa9d19d08422962316c58b0341ed938285855b4b",
    "block_hash": "024409a272dd2f87b0c98f8be109dbf993eb61346a0f4ff5dacc5a7205b49623",
    "dag_block_hash_as_pivot": "0b7986a9c8cc7677264a161f2e3622f57ff63aeb78fad1dcac4b32655aaab5ea",
    "order_hash": "9ee61bed15a723b6bc4178a30e3748635141e8b8cb2bd0ace398f1b65519cb08",
    "period": 1921851,
    "prev_block_hash": "664a895a8d030bdbe27e853641150915421c2418301e20f763d61746686c1d73",
    "schedule": {
      "dag_blocks_order": [
        "0x64c4e3f6380e8c85ab3a5cb497f19831e6b355d6007ff04af201e3a421d43703",
        "0x5847486c760d760eecb9289e75313eaa7bb6270fc39f5d94bacd6a62511ff58a",
        "0x687bdda424f51c87321e96c05d93717880d8a1360f9e36f43994fc93b47ccba7",
        "0xb6f1b364af58c343960882ce634e0146854277ec53547cc6014f0b8f0e6facb8",
        "0xda50bf40050c07ca4a683aebb3c9f88429f9d9fa3de56202277644fe82a16cf5",
        "0x508ba74261788c516e9f37074ff7eed427f1f385200dee228174d32379710c36",
        "0x4960ba2d0dad2656d63476eb84f703209b63406d9b36686dc6104fa87883cb48",
        "0x418670a0bc42e771f6e6094c1fd3759a37080fc0328580c244ea5702622e3149",
        "0x347ca32618309f37bd7efd2291a1f4aea70b9df1a1d6d5e1d5ecb21645e530e9",
        "0x04712c69a8b572eb3cb9118df41dd481c22467d76c2313eed8fe917ca4e495c0",
        "0xd48b200ee5d56ecaa97ca4456c573d10e0966f2114481cf21fd041b52141d24c",
        "0x0e9af05187db2043d87d54d2c2393fb4611e9c25c8774cee349bdce13650feb0",
        "0x8ca87291fecdfc4e75d946f24bcbe28d38abe470d23f456b9e6aabcc3114fce0",
        "0x9a277ade0123a9963efc85c03e3773903c9ff7bfb8c33ffbcdf05aa0c24040cc",
        "0xf8ad8dc55eba458540d3297bdf1bc7304c0b4e4a03c41f49a30f8e2b4a06e314",
        "0xbf763c19a3625e097fc9cc162f584379dc7fe6bf1329d3f887bb2079240a4e1c",
        "0xa328ef31b261ddc332ddb3dbff45d9aae4caa1a405dc580af12a91a357bed8f0",
        "0x1e843649f167f5180a0017e106e124477ad0c4e7ae6ca95ac1b17168c4eedd43",
        "0xd5305ce0bb792b0d98e47bd3cdc7f33f1d23e9309b63c174ebcb2be07b0248e1",
        "0x0b7986a9c8cc7677264a161f2e3622f57ff63aeb78fad1dcac4b32655aaab5ea"
      ]
    },
    "signature": "c95b29cad2dc3cd6987083a0782c90cc0e25b45ab353ee21f22ef0ed059077d26e14bfe7cc9401ac6dcd101a071951e06ce7d739094c2519e5a151c06ceb96a600",
    "timestamp": 1653563053
  }
}
```

### taraxa_pbftBlockHashByPeriod

Returns hash of PBFT block for specified period

#### Parameters

`QUANTITY` - period

#### Returns

`STRING` - PBFT block hash

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_pbftBlockHashByPeriod","params":["0x1D533B"],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": "024409a272dd2f87b0c98f8be109dbf993eb61346a0f4ff5dacc5a7205b49623"
}
```

### taraxa_getConfig

Returns current node configuration

#### Parameters

none

#### Returns

`OBJECT` - config object
* `chain_id`: `QUANTITY` - chain id. Different between networks
* `dag_genesis_block`: `OBJECT` - DAG genesis block
* `state`: `OBJECT` - State config object
* `pbft`: `OBJECT` - PBFT configuration object
* `sortition`: `OBJECT` - Sortition configuration object

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"taraxa_getConfig","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "chain_id": "0x1",
    "dag_genesis_block": {
      "level": "0x0",
      "pivot": "0x0000000000000000000000000000000000000000000000000000000000000000",
      "sig": "0xb7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
      "timestamp": "0x61cd9f40",
      "tips": [],
      "transactions": []
    },
    "state": {
      "dpos": {
        "deposit_delay": "0x5",
        "eligibility_balance_threshold": "0x186a0",
        "genesis_state": {
          "0x0274cfffea9fa850e54c93a23042f12a87358a82": {
            "0x00ccd0de0809ac03fd292036ee1544185583cd88": "0x989680",
            "0x0d149e61cc02b5893ef6fc33bc7d67ff13eeeee0": "0x989680",
            "0x1f8333245650a19a0683891b7afe7787a3ce9f00": "0x989680",
            "0x267e780b7843992e57f52e13018f0f97467ac06e": "0x989680",
            "0x6f96be7626a74e86c76e65ccbccf0a38e2b62fc5": "0x989680",
            "0x9d047654e55248ec38aa6723a5ab36d171008584": "0x989680",
            "0xd20131f980c9932b1df31cf3aafeecfb1d504381": "0x989680",
            "0xd4e4728bea5a67dd70dccb742bdc9c3a48465bec": "0x989680",
            "0xec591a85f613fe98f057dc09712a9b22cdd05845": "0x989680"
          }
        },
        "vote_eligibility_balance_step": "0x186a0",
        "withdrawal_delay": "0x5"
      },
      "initial_balances": {
        "0x0274cfffea9fa850e54c93a23042f12a87358a82": "0x141e8d17",
        "0x111f91441efc8c6c0edf6534970cc887e2fabaa8": "0x24048ce3d"
      },
      "hardforks": {
        "fix_genesis_fork_block": "0x102ca0"
      }
    },
    "pbft": {
      "committee_size": "0x3e8",
      "dag_blocks_size": "0xa",
      "ghost_path_move_back": "0x0",
      "lambda_ms": "0x5dc",
    },
    "sortition": {
      "changes_count_for_average": "0x5",
      "changing_interval": "0x0",
      "computation_interval": "0xc8",
      "dag_efficiency_targets": [
        "0x12c0",
        "0x1450"
      ],
      "max_interval_correction": "0x3e8",
      "vdf": {
        "difficulty_max": "0x12",
        "difficulty_min": "0x10",
        "difficulty_stale": "0x14",
        "lambda_bound": "0x64",
        "stake_threshold": "0x4e20"
      },
      "vrf": {
        "threshold_upper": "0x1770"
      }
    }
  }
}
```

## Test API

### get_sortition_change

Get sortition params changes for closest smaller period to specified

#### Parameters

`OBJECT` - param is wrapped into object
* `period`: QUANTITY` - period number to get sortition parameters for

#### Returns

`OBJECT` - sortition parameters
* `interval_efficiency`: `QUANTITY` - efficiency of interval
* `period`: `QUANTITY` - number of period when closest smaller change happened
* `threshold_upper`: `QUANTITY` - upper bound of selection
* `kThresholdUpperMinValue`: `QUANTITY` - const that represents minimum value of `threshold_upper`

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"get_sortition_change","params":[{"period": 3000}],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "interval_efficiency": 1327,
    "kThresholdUpperMinValue": 80,
    "period": 2820,
    "threshold_upper": 31953
  }
}
```

### send_coin_transaction

Method to create and send transfer transaction. Used in some tests and CI

#### Parameters

`OBJECT` - block json to insert
* `secret`: `DATA`, 32 Bytes - Secret key for account to send transaction from
* `nonce`: `QUANTITY` - Nonce to send transaction with
* `value`: `QUANTITY` - Value to specify in created transaction
* `gas_price`: `QUANTITY` - Gas price to specify in created transaction
* `gas`: `QUANTITY` - Amount of gas transaction can use
* `receiver`: `DATA`, 20 Bytes - Address of receiver account

#### Returns

none

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"send_coin_transaction","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": null
}
```
### send_coin_transactions

Method to create and send multiple transaction. Used to create high transactions load in tests

#### Parameters

`OBJECT` - block json to insert
* `secret`: `DATA`, 32 Bytes - Secret key for account to send transactions from
* `nonce`: `QUANTITY` - Nonce to start sending transaction with
* `value`: `QUANTITY` - Value to specify in created transaction
* `gas_price`: `QUANTITY` - Gas price to specify in created transaction
* `gas`: `QUANTITY` - Amount of gas transaction can use
* `receiver`: `DATA`, 20 Bytes - Array of Addresses of receiver account
* `transaction_count`: `QUANTITY`, - Number of transactions to create

#### Returns

Number of transactions inserted successfully, in case of transaction insertion failure, error message is returned



### get_account_address

Returns address of account on current node

#### Parameters

none

#### Returns

`DATA`, 20 Bytes - address of account on current node

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"get_account_address","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "value": "b3888b0bafcf1f9481a0ec97ff032d84f22254ec"
  }
}
```

### get_peer_count

Returns count of peers that node is connected to

#### Parameters

none

#### Returns

`QUANTITY` - count of peers that node is connected to

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"get_peer_count","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "value": 50
  }
}
```

### get_node_status

Returns current taraxa network protocol version

#### Parameters

none

#### Returns

`OBJECT` - with a lot of info about node status including networking. See Example

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"get_node_status","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "blk_count": 6033325,
    "blk_executed": 6033312,
    "blk_queue_size": 0,
    "dag_level": 3058195,
    "dpos_node_votes": 0,
    "dpos_quorum": 667,
    "dpos_total_votes": 15300,
    "network": {
      "peer_max_node_dag_level": 3058195,
      "peer_max_node_dag_level_node_id": "26d873a8fe9f3fe763491bfb1e0fcb3e95c7f5ca0d791f22936f62ba1ece9f0384c347f0ed642f997fe93203781af8116180458f66ed27fa58e93cd7fddc8947",
      "peer_max_pbft_chain_size": 324432,
      "peer_max_pbft_chain_size_node_id": "dd607f2ec9daa9f47335d0da859ebc2d1b489085c8b918f5ca3f13c0c8e6a29b731be4adf0c9c729baf932a656bcc4b6c9e2b10e9aa906b170f2c271504ababc",
      "peer_max_pbft_round": 324617,
      "peer_max_pbft_round_node_id": "26d873a8fe9f3fe763491bfb1e0fcb3e95c7f5ca0d791f22936f62ba1ece9f0384c347f0ed642f997fe93203781af8116180458f66ed27fa58e93cd7fddc8947",
      "peers": [
        {
          "dag_level": 3058195,
          "dag_synced": true,
          "node_id": "26d873a8fe9f3fe763491bfb1e0fcb3e95c7f5ca0d791f22936f62ba1ece9f0384c347f0ed642f997fe93203781af8116180458f66ed27fa58e93cd7fddc8947",
          "pbft_size": 324431
        }, ...
      ]
    },
    "node_count": 79,
    "pbft_round": 324617,
    "pbft_size": 324432,
    "pbft_sync_period": 324432,
    "pbft_sync_queue_size": 0,
    "peer_count": 50,
    "synced": true,
    "syncing_seconds": 0,
    "trx_count": 28017090,
    "trx_executed": 28017007,
    "trx_nonfinalized_size": 83,
    "trx_pool_size": 7
  }
}
```

### get_all_nodes

Returns info of current connected nodes including its id, ip_address and port

#### Parameters

none

#### Returns

`STRING` - string that contains info about peer if format `id ip_address:port` separated with `\n`.

#### Example

```json
// Request
curl -X POST --data '{"jsonrpc":"2.0","method":"get_all_nodes","params":[],"id":1}'

// Result
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": "f8ff62f2df819566023ec3cda97f73697e789089b5c97c22a24c175edec3256242796a7a843fe1b949558160dae0fd5510e31c93440798212ef99a55c64e7b0f 66.94.114.186:10002\n45022ea1765d3a569003e2088a0893ffe00656c25f32ee414b39c6e45c8aa05cc41453158d832c67b5bba710edb648223f654fae9e2a1f513d7e42447f25a2d6 38.242.218.203:10002"
}
```
