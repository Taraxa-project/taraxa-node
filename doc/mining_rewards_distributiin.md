# Mining rewards distribution algorithm
The main goal of fair rewards distribution is to motivate miners to participate in all steps of the mining process
(creating high-quality DAG blocks, creating PBFT blocks, voting for PBFT blocks) as much as possible.


## Taraxa architecture brief summary
It is important to understand some basic principles that are used in taraxa ledger before getting into the mining rewards:

* DAG (Direceted Acyclic Graph) structure is used to store "DAG" blocks(equivalent to the classic blocks, e.g. in Ethereum),
* In case network works under normal conditions (no fractioning, etc...) PBFT consensus takes place approx. every 10 seconds (TODO: confirm), which
ulitmately determines the order of the dag blocks(for further detals please read this [series of medium articles](https://medium.com/taraxa-project/tagged/taraxa-tech)),
* When PBFT consenus pass, final block is created out of ordered transactions(taken from ordered DAG blocks) and pushed 
  to the taraxa final chain
  
### Beneficial work in network
Considering previous section, there are 3 types of beneficial work that result in stable network functioning:

* Proposing high-quality DAG blocks(for example block with 1000 unique transactions has bigger value than the block 
  with 5 transactions),
* Proposing PBFT blocks,
* Voting for PBFT blocks


## Rewards sources
There are 2 sources of tokens, which are added up and later distributed as rewards:

* Newly created (mined) tokens - fixed according to predefined rewards schedule,
        TODO: what is this predefined schedule in taraxa ?  
* Included transactions fees - this number varies a lot. There might be a big difference between blocks as it depends 
  on total number of transactions included in block as well as the types of these transactions (different transaction types cost different fee).
  

## Rewards distribution

To propose the DAG or PBFT block, miner has to provide vrf/vdf proof that he is actually eligible to do that, which
is a work that should be rewarded as well as PBFT votes. Proposing blocks + voting is essential. All mined tokens
are therefore reserved for this work. 

What actually really separates high-quality miners is number of transactions included in the block. Block with only 
5 transactions is less valuable than block with 1000 transactions... Included transactions fees are therefore distributed 
based on the transactions each miner mined (included in his blocks).

### Newly created (mined) tokens:

* **50%** goes proportionally to the **DAG blocks proposers**, each valid DAG block = 1 share. For example let's have
  10 blocks in current PBFT round, 4 were mined by miner A and 6 by miner B. Miner A gets 4/10 out of all tokens 
  reserved for DAG block proposers, miner B gets 6/10,
* **25%** goes to the current **PBFT block proposer**,
* [non-actual] **25%** goes to the **PBFT block voters** - TODO: provide some way how to track pbft votes, might be some
  historical stats, etc... At the moments these 25% of tokens goes to the block PBFT block proposer

### Included transactions fees:

Each transaction has a different fee. Simple "token send tx" has different fee than "smart contract write tx". These
fees are calculated in taraxa-evm. 

Block proposer is rewarded accordingly based on what transactions he included in his block. 
It might motivate miners to prioritize transactions with higher fees, on the other hand if there would be equal reward for 
all transactions types, it would motivate miners to prioritize more simple transactions (which require less work to process).
This way there is also a possibility for users to try to make their transactions being processed faster by specifying higher gas price. 

* **100%** goes to the **DAG blocks proposers** based on transactions they included in blocks.

Note that the same transaction might be included in multiple blocks. In such case:

* 75% of transaction fee goes to the miner, who included it as first
* 25% is divided between other miners who included it as second

**Important:** it is open for discussion if only transactions that are actually executed(included first) should be rewarded and
uncle transactions (already included by some other miner before) should get some smaller proportion or it is equal.


## Miners statistics

Rewards distribution described in the previous section is based on miners statistic in current PBFT round.
Such statistics consists of:
* PBFT block author
* for each miner:
    * Number of proposed dag blocks,
    * Number of unique executed transactions,
    * Number of uncle transactions (already included in other miner's block, which is ordered as first),
    * Voting flag

#### Example:

Let's have following DAG structure:

![Unoreded DAG blocks](./images/DAG_unordered.png?raw=true "Unoreded DAG blocks")

, now let's say PBFT consensus is successfully executed on this DAG and it is ordered as follows:

![Oreded DAG blocks](./images/DAG_ordered.png?raw=true "Oreded DAG blocks")

(note that order of blocks here is just illustrative, to check real ordering mechanism,
please see [this article](https://medium.com/taraxa-project/taraxa-consensus-3-5-secure-and-fair-block-dag-ordering-ed4203420ac6)).
Unique transactions are taken from this temporary chain and included in single block that is pushed into the final chain.

, from these ordered blocks, following miners statistic can be derived:

![Miners_statistics](./images/miners_statistics.png?raw=true "Miners statistics")


# Important:

This is just a proposal, which might be changed from specified % values to main principles that are used for calculations... 
It is a first version, from which we can take off and start further discussion...

