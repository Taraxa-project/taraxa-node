# Rewards distribution algorithm

The main goal of fair rewards distribution is to motivate validators to participate in all steps of the validation process(creating high-quality DAG blocks, creating PBFT blocks, voting for PBFT blocks) as much as possible.

## Glossary

* DAG blocks are stored stored in DAG(Directed Acyclic Graph) structure and have transactions inside it. 
* PBFT block is finalizing part of ordered DAG blocks from DAG structure and transactions from them. 
* Reward votes is finalized votes from PBFT block. Block `N` is finalizing votes from block `N - 1`.

More info could be found in this [series of medium articles](https://medium.com/taraxa-project/tagged/taraxa-tech)

## Rewards sources

There are 2 sources of tokens, which are distributed as rewards:
* Newly created tokens. Rewards depends on total delegations in the network and is equal to 20% of it per year. So block reward is equal to `total_delegated_amount * 20% / block_per_year`. It could change from block to block as total delegated amount is changing.
* Transactions fees is basically transaction cost. Rewards for block depends on transactions that are included in DAG block and its uniqueness.

## Rewards distribution

### Beneficial work in network

There are 3 types of beneficial work that result in stable network functioning:

* Proposing high-quality DAG blocks(for example block with 10 unique transactions has bigger value than the block with 5 transactions),
* Proposing PBFT blocks
* Voting for PBFT blocks

### Newly created tokens:

* **80%** goes proportionally to the **DAG blocks proposers**, according to block weight calculated from count of unique transactions. For example PBFT finalizes 2 DAG blocks. The first one have 3 transactions and the second have 7. So first will receive 3/10 of this reward and second one will receive 7/10 of this reward.
* **10%** or less goes to **PBFT block proposer**, proportional to bonus reward votes count. Bonus reward votes are all votes over `2/3 + 1` needed threshold. So that means that **PBFT block proposer** will receive reward only for this additional votes and will receive all **10%** of reward only if all votes will be included. In other case part that was left will be burned.
* **10%** goes to the **PBFT block voters** from reward votes list and calculated according to their vote weight.

### Included transactions fees:

Each transaction has a different fee. Simple "token send tx" has different fee than "smart contract write tx". These fees are calculated in taraxa-evm. 

* **100%** goes to the **DAG blocks proposers**

Block proposer is rewarded for unique transactions included into DAG block. Unique transaction means that proposer is the first one who included it in the DAG block according to DAG order. It might motivate validators to not include transactions that was already included by other proposers and to prioritize transactions with higher fees. On the other hand if there would be equal reward for all transaction types, it would motivate validators to prioritize more simple transactions (which require less work to process). This way there is also a possibility for users to try to make their transactions being processed faster by specifying higher gas price. 


## Validators statistics

Rewards distribution described in the previous section is based on validators statistic in current PBFT round. Such statistics consists of:
* PBFT block author
* Total unique transactions count 
* Total votes weight 
* Max block votes weights
* for each validator:
    * Number of included unique transactions 
    * Vote weight 

#### Example:
