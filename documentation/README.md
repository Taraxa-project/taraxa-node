# Overview

This directory houses documentation for tools related to taraxa.
The tools either produce output to standard output and/or consume
input on standard input in text format, usually JSON.
All programs assume that input and output encoded in hex format
doesn't have the leading 0x.

## Key generation

Programs
[generate_private_key](generate_private_key.md),
[print_key_info](print_key_info.md) and
[generate_private_key_from_seed](generate_private_key_from_seed.md)
handle generation of private and public keys of accounts.
The first generates a random key and prints it to standard output
while the second one reads a private key from standard input and
prints it and corresponding public key to standard output.
The third program generates deterministic but unpredictable
private keys from a seed given by the user.
This is comparable to btc HD wallet, iota wallet, etc.
Private and public keys are used by all other taraxa tools for
signing and verifying transactions, etc.

* to create a private key run `generate_private_key`, keep the printed key safe
* to find out your public key give your private key to `print_key_info`


## Signatures

Programs [sign_message](sign_message.md) and [verify_message](verify_message.md)
can be used to sign messages with a private key and to verify
message signatures with corresponding public key.

* to sign a message run `sign_message` and give it your private key and the message
* to verify a signature run `verify_message` and give it the message and the public key, which is obtained from private key with `print_key_info`


## Creating transactions and votes

Programs [create_send](create_send.md), [create_receive](create_receive.md) and [create_transient_vote](create_transient_vote.md)
are used for creating transactions in taraxa and for consensus
in case of conflicting transactions. Send transactions can
carry arbitrary payload which can be used e.g. for smart contracts.
These program print their results to standard output.
You can use programs described in ledger operations to store transactions and votes permanently.

* to send coins from account A to account B:
  * A runs `create_send` giving it address of B
  * B runs `create_receive` giving it hash of above send
* to vote for a transactionwith your balance, in order to resolve conflicts, run `create_transient_vote` giving it hash of your candidate


## Ledger operations

Programs [add_transaction](add_transaction.md) and [replace_transaction](replace_transaction.md)
are used to create a persistend ledger of transactions and votes
created by various create_* programs. replace_transaction does
everything that add_transaction can do.

* to validate a transaction and add it to local storage run `add_transaction` giving it the transaction, path to local ledger and other info
* to replace a transaction in local storage with another one that has more votes run `replace_transaction` giving it the new transaction, path to local ledger, etc.
  * you can vote for transactions with `create_transient_vote` and add the vote to local ledger with `add_vote`


## Payload serialization (smart contracts)

[serialize_payloads](serialize_payloads.md) program iterates
through persistent ledger and prints to standard output all
payloads stored in transactions. Order in which payloads are
printed is defined by relative ordering of all send+receive
pairs of ledger.

TODO: better payload serialization that only enforces required
ordering between transaction payloads

* to print the payload of all transactions in local ledger run `serialize_payloads` giving it the path of local ledger
  * order of payloads is defined by the ledger DAG, payloads of transactions that aren't ordered relative to each other by DAG are printed in random order with respect to each other
