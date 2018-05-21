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

## Signatures

Programs [sign_message](sign_message.md) and [verify_message](verify_message.md)
can be used to sign messages with a private key and to verify
message signatures with corresponding public key.

## Creating transactions and votes

Programs [create_send](create_send.md), [create_receive](create_receive.md) and [create_transient_vote](create_transient_vote.md)
are used for creating transactions in taraxa and for consensus
in case of conflicting transactions. Send transactions can
carry arbitrary payload which can be used e.g. for smart contracts.
These program print their results to standard output.
You can use programs described in ledger operations to store transactions and votes permanently.

## Ledger operations

Programs [add_transaction](add_transaction.md) and [replace_transaction](replace_transaction.md)
are used to create a persistend ledger of transactions and votes
created by various create_* programs.

## Payload serialization (smart contracts)

[serialize_payloads](serialize_payloads.md) program iterates
through persistent ledger and prints to standard output all
payloads stored in transactions. Order in which payloads are
printed is defined by relative ordering of all send+receive
pairs of ledger.

TODO: better payload serialization that only enforces required
ordering between transaction payloads
