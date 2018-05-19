Reads a vote from standard input and adds it to ledger storage.
Votes can be created with [create_transient_vote](create_transient_vote.md)
```bash
$ ../create_transient_vote \
    --latest 0000000000000000000000000000000000000000000000000000000000000000 \
    --candidate 0000000000000000000000000000000000000000000000000000000000000000 \
    --key 0000000000000000000000000000000000000000000000000000000000000001 \
| ../add_vote --ledger-path ledger_data
```
