Replaces (or adds) transactions to ledger data based on votes in ledger data.
After running commands in [add_transaction](add_transaction.md):

Create a send of one coin to another account that also refers to genesis transaction
of this account:
```bash
../create_send \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --new-balance 8 \
  --receiver 025ECBE4D1A6330A44C8F7EF951D4BF165E6C6B721EFADA985FB41661BC6E7FD6C \
  --previous 8EC16B869DEEA4E5F1823CDA8B9BFD5B5558AAB9DF9448AB9EFA0B02AEA70740 \
  --payload '' \
  > send2
```

Add the corresponding receive to ledger:
```bash
../create_receive \
  --key 0000000000000000000000000000000000000000000000000000000000000003 \
  --previous 0000000000000000000000000000000000000000000000000000000000000000 \
  --send 324ADE3C21261CD810C555EC7C24683FEAA1B19E3F7FC96078B241C0CA91CAAD \
  > receive2
```

Append receive to ledger:
```bash
../append_to_ledger --ledger-path ledger_data < receive2
```

Add a vote from sending account to second receive, with genesis as latest valid
transaction:
```bash
../create_transient_vote \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --latest 0000000000000000000000000000000000000000000000000000000000000000 \
  --candidate 324ADE3C21261CD810C555EC7C24683FEAA1B19E3F7FC96078B241C0CA91CAAD
```

Replace first send+receive with second one:
```bash
../replace_transaction --ledger-path ledger_data < send2
```
