# Rewards distribution algorithm

The main goal of fair rewards distribution is to motivate validators to participate in all steps of the validation process(creating high-quality DAG blocks, creating PBFT blocks, voting for PBFT blocks) as much as possible.

## Glossary

* DAG blocks are stored in DAG(Directed Acyclic Graph) structure and have transactions inside it. 
* PBFT block is finalizing part of ordered DAG blocks from DAG structure and transactions from them. 
* Reward votes is finalized votes from PBFT block. Block `N` is finalizing votes from block `N - 1`.

More info could be found in this [series of medium articles](https://medium.com/taraxa-project/tagged/taraxa-tech)

## Rewards sources

There are 2 sources of tokens, which are distributed as rewards:
* Newly created tokens. Rewards depend on total delegations in the network and is equal to 20% of it per year. So block reward is equal to `total_delegated_amount * 20% / block_per_year`. It could change from block to block as total delegated amount is changing.
* Transactions fees is basically transaction cost. Rewards for block depends on transactions that are included in DAG block and its uniqueness.

## Rewards distribution

### Beneficial work in network

There are 3 types of beneficial work that result in stable network functioning:

* Proposing high-quality DAG blocks(for example block with 10 unique transactions has bigger value than the block with 5 transactions),
* Proposing PBFT blocks
* Voting for PBFT blocks

### Newly created tokens:

* **50%** goes proportionally to the **DAG blocks proposers**, according to block weight calculated from count of unique transactions. For example PBFT finalizes 2 DAG blocks. The first one have 3 transactions and the second have 7. So first will receive 3/10 of this reward and second one will receive 7/10 of this reward.
* **45%** of total votes reward is voters part. So voters from reward votes will receive reward calculated according to their vote weight.
* **5%** of total votes reward is part for **PBFT block proposer**. It is proportional to bonus reward votes count. Bonus reward votes are all votes over `2/3 + 1` needed threshold. So that means that **PBFT block proposer** will receive reward only for this additional votes and will receive all **10%** of reward only if all votes will be included. The rest of reward will be burned if included votes count is less than maximum possible.

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

## Example:

### DAG structure:

![Unoreded DAG blocks](./images/DAG_unordered.png?raw=true "Unordered DAG blocks")

### PBFT block

Let's say PBFT consensus was reached on PBFT block produced by `Validator A` on this DAG and it is ordered as follows:

![Oreded DAG blocks](./images/DAG_ordered.png?raw=true "Ordered DAG blocks")

Block votes:
1. Validator A weight 1
2. Validator B weight 3
3. Validator C weight 2
4. Validator D weight 4

### Statistics 

1. PBFT block author is `Validator A`
2. Total unique transactions count is `8`
3. Total votes weight is `10`
4. Max block votes weights is `11`
5. Validator statistics:

Validator   | Rewardable DAG blocks | Vote weight |
----------  | ------------------- | ----------- | 
Validator A | 1 | 1 |
Validator B | 2 | 3 |
Validator C | 0 | 2 |
Validator D | 1 | 4 |
Validator E | 1 | 0 |

### Rewards

Newly generated tokens amount per year is equal to total delegated amount multiplied by yield percentage. So every new block will create a reward is equal to newly generated tokens amount per year divided by estimated blocks count per year. All following calculations would be described with a pseudocode. Let's put some random values to show the calculations:
```
total_delegated_amount = 10_000_000_000
yield_percentage = 20%
new_tokens_per_year = total_delegated_amount * yield_percentage // 2_000_000_000
blocks_per_year = 20_000
block_generated_reward = new_tokens_per_year / blocks_per_year // 100_000
```

#### DAG blocks rewards

Voters rewards calculation
```
dag_blocks_reward_part = block_generated_reward * 50% // 50_000
total_rewardable_dag_count = 5

validator_A_dag_count = 1
validator_A_dag_reward = dag_blocks_reward_part * validator_A_dag_count / block_total_votes_weight // 5_000

validator_B_dag_count = 3
validator_B_dag_reward = dag_blocks_reward_part * validator_B_dag_count / block_total_votes_weight // 15_000

validator_C_dag_count = 0
validator_C_dag_reward = 0

validator_D_dag_count = 2
validator_C_dag_reward = dag_blocks_reward_part * validator_C_dag_count / block_total_votes_weight // 10_000

validator_E_dag_count = 4
validator_E_dag_reward = dag_blocks_reward_part * validator_E_dag_count / block_total_votes_weight // 20_000
```

#### PBFT proposer reward

PBFT proposer gets reward for bonus votes, as described above. We have 5% in config, so:
```
max_pbft_proposer_reward = block_generated_reward * 5% // 5_000
block_max_votes_weight = 11
two_t_plus_one = block_max_votes_weight * 2 / 3 + 1 // 8
block_total_votes_weight = 10
block_bonus_reward_votes = block_total_votes_weight - two_t_plus_one // 2
block_total_bonus_reward_votes = block_max_votes_weight - two_t_plus_one // 3
proposer_reward = max_proposer_reward * block_bonus_reward_votes / block_total_bonus_reward_votes // 3_333
burned_amount = max_proposer_reward - proposer_reward // 1667
```

#### PBFT voters reward

Rewards for the votes is calculated from left part. 100% - 50% - 5% = 45%. So we have this calculations:
```
pbft_voters_reward = block_generated_reward * 45% // 45_000
block_total_votes_weight = 10

validator_A_vote_weight = 1
validator_A_vote_reward = pbft_voters_reward * validator_A_vote_weight / block_total_votes_weight // 4_500

validator_B_vote_weight = 3
validator_B_vote_reward = pbft_voters_reward * validator_B_vote_weight / block_total_votes_weight // 13_500

validator_C_vote_weight = 2
validator_C_vote_reward = pbft_voters_reward * validator_C_vote_weight / block_total_votes_weight // 9_000

validator_D_vote_weight = 4
validator_D_vote_reward = pbft_voters_reward * validator_D_vote_weight / block_total_votes_weight // 18_000

validator_E_vote_weight = 0
validator_E_vote_reward = 0
```

