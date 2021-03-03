# Schema Types

<details>
  <summary><strong>Table of Contents</strong></summary>

  * [Query](#query)
  * [Mutation](#mutation)
  * [Objects](#objects)
    * [Account](#account)
    * [Block](#block)
    * [CallResult](#callresult)
    * [Log](#log)
    * [Pending](#pending)
    * [SyncState](#syncstate)
    * [Transaction](#transaction)
  * [Inputs](#inputs)
    * [BlockFilterCriteria](#blockfiltercriteria)
    * [CallData](#calldata)
    * [FilterCriteria](#filtercriteria)
  * [Scalars](#scalars)
    * [Address](#address)
    * [BigInt](#bigint)
    * [Boolean](#boolean)
    * [Bytes](#bytes)
    * [Bytes32](#bytes32)
    * [Int](#int)
    * [Long](#long)
    * [String](#string)

</details>

## Query
<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>block</strong></td>
<td valign="top"><a href="#block">Block</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">number</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">hash</td>
<td valign="top"><a href="#bytes32">Bytes32</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>blocks</strong></td>
<td valign="top">[<a href="#block">Block</a>!]!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">from</td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">to</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>pending</strong></td>
<td valign="top"><a href="#pending">Pending</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transaction</strong></td>
<td valign="top"><a href="#transaction">Transaction</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">hash</td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>logs</strong></td>
<td valign="top">[<a href="#log">Log</a>!]!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">filter</td>
<td valign="top"><a href="#filtercriteria">FilterCriteria</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasPrice</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>syncing</strong></td>
<td valign="top"><a href="#syncstate">SyncState</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>chainID</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
</tbody>
</table>

## Mutation
<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>sendRawTransaction</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">data</td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
</tbody>
</table>

## Objects

### Account

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>address</strong></td>
<td valign="top"><a href="#address">Address</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>balance</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactionCount</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>code</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>storage</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">slot</td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
</tbody>
</table>

### Block

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>number</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>hash</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>parent</strong></td>
<td valign="top"><a href="#block">Block</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>nonce</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactionsRoot</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactionCount</strong></td>
<td valign="top"><a href="#int">Int</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>stateRoot</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>receiptsRoot</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>miner</strong></td>
<td valign="top"><a href="#account">Account</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">block</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>extraData</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasLimit</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasUsed</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>timestamp</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>logsBloom</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>mixHash</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>difficulty</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>totalDifficulty</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>ommerCount</strong></td>
<td valign="top"><a href="#int">Int</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>ommers</strong></td>
<td valign="top">[<a href="#block">Block</a>]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>ommerAt</strong></td>
<td valign="top"><a href="#block">Block</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">index</td>
<td valign="top"><a href="#int">Int</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>ommerHash</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactions</strong></td>
<td valign="top">[<a href="#transaction">Transaction</a>!]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactionAt</strong></td>
<td valign="top"><a href="#transaction">Transaction</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">index</td>
<td valign="top"><a href="#int">Int</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>logs</strong></td>
<td valign="top">[<a href="#log">Log</a>!]!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">filter</td>
<td valign="top"><a href="#blockfiltercriteria">BlockFilterCriteria</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>account</strong></td>
<td valign="top"><a href="#account">Account</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">address</td>
<td valign="top"><a href="#address">Address</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>call</strong></td>
<td valign="top"><a href="#callresult">CallResult</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">data</td>
<td valign="top"><a href="#calldata">CallData</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>estimateGas</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">data</td>
<td valign="top"><a href="#calldata">CallData</a>!</td>
<td></td>
</tr>
</tbody>
</table>

### CallResult

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>data</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasUsed</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>status</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
</tbody>
</table>

### Log

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>index</strong></td>
<td valign="top"><a href="#int">Int</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>account</strong></td>
<td valign="top"><a href="#account">Account</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">block</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>topics</strong></td>
<td valign="top">[<a href="#bytes32">Bytes32</a>!]!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>data</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transaction</strong></td>
<td valign="top"><a href="#transaction">Transaction</a>!</td>
<td></td>
</tr>
</tbody>
</table>

### Pending

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>transactionCount</strong></td>
<td valign="top"><a href="#int">Int</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>transactions</strong></td>
<td valign="top">[<a href="#transaction">Transaction</a>!]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>account</strong></td>
<td valign="top"><a href="#account">Account</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">address</td>
<td valign="top"><a href="#address">Address</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>call</strong></td>
<td valign="top"><a href="#callresult">CallResult</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">data</td>
<td valign="top"><a href="#calldata">CallData</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>estimateGas</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">data</td>
<td valign="top"><a href="#calldata">CallData</a>!</td>
<td></td>
</tr>
</tbody>
</table>

### SyncState

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>startingBlock</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>currentBlock</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>highestBlock</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>pulledStates</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>knownStates</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
</tbody>
</table>

### Transaction

<table>
<thead>
<tr>
<th align="left">Field</th>
<th align="right">Argument</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>hash</strong></td>
<td valign="top"><a href="#bytes32">Bytes32</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>nonce</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>index</strong></td>
<td valign="top"><a href="#int">Int</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>from</strong></td>
<td valign="top"><a href="#account">Account</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">block</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>to</strong></td>
<td valign="top"><a href="#account">Account</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">block</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>value</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasPrice</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gas</strong></td>
<td valign="top"><a href="#long">Long</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>inputData</strong></td>
<td valign="top"><a href="#bytes">Bytes</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>block</strong></td>
<td valign="top"><a href="#block">Block</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>status</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasUsed</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>cumulativeGasUsed</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>createdContract</strong></td>
<td valign="top"><a href="#account">Account</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" align="right" valign="top">block</td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>logs</strong></td>
<td valign="top">[<a href="#log">Log</a>!]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>r</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>s</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>v</strong></td>
<td valign="top"><a href="#bigint">BigInt</a>!</td>
<td></td>
</tr>
</tbody>
</table>

## Inputs

### BlockFilterCriteria

<table>
<thead>
<tr>
<th colspan="2" align="left">Field</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>addresses</strong></td>
<td valign="top">[<a href="#address">Address</a>!]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>topics</strong></td>
<td valign="top">[[<a href="#bytes32">Bytes32</a>!]!]</td>
<td></td>
</tr>
</tbody>
</table>

### CallData

<table>
<thead>
<tr>
<th colspan="2" align="left">Field</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>from</strong></td>
<td valign="top"><a href="#address">Address</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>to</strong></td>
<td valign="top"><a href="#address">Address</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gas</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>gasPrice</strong></td>
<td valign="top"><a href="#bigint">BigInt</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>value</strong></td>
<td valign="top"><a href="#bigint">BigInt</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>data</strong></td>
<td valign="top"><a href="#bytes">Bytes</a></td>
<td></td>
</tr>
</tbody>
</table>

### FilterCriteria

<table>
<thead>
<tr>
<th colspan="2" align="left">Field</th>
<th align="left">Type</th>
<th align="left">Description</th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" valign="top"><strong>fromBlock</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>toBlock</strong></td>
<td valign="top"><a href="#long">Long</a></td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>addresses</strong></td>
<td valign="top">[<a href="#address">Address</a>!]</td>
<td></td>
</tr>
<tr>
<td colspan="2" valign="top"><strong>topics</strong></td>
<td valign="top">[[<a href="#bytes32">Bytes32</a>!]!]</td>
<td></td>
</tr>
</tbody>
</table>

## Scalars

### Address

### BigInt

### Boolean

The `Boolean` scalar type represents `true` or `false`.

### Bytes

### Bytes32

### Int

The `Int` scalar type represents non-fractional signed whole numeric values. Int can represent values between -(2^31) and 2^31 - 1.

### Long

### String

The `String` scalar type represents textual data, represented as UTF-8 character sequences. The String type is most often used by GraphQL to represent free-form human-readable text.

