# Basics

Create a send transaction without payload, self as receiver, previous as 0 to signify genesis transaction and without payload:
```bash
./create_send \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --new-balance 9 \
  --receiver 6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C2964FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5 \
  --previous 0000000000000000000000000000000000000000000000000000000000000000 \
  --payload '' \
  > genesis_transaction
```
You can use `echo 0000000000000000000000000000000000000000000000000000000000000001 | ./print_key_info` to find out the public key and the address of above key.


Add this transaction to the ledger located in ledger_data directory:
```bash
./append_to_ledger --ledger-path ledger_data < genesis_transaction
```

Create a send of one coin to another account with the hash of the above transaction as previous and the address of target account as receiver:
```bash
./create_send \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --new-balance 8 \
  --receiver 7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC4766997807775510DB8ED040293D9AC69F7430DBBA7DADE63CE982299E04B79D227873D1 \
  --previous A61A5A801BD537E613CC6D48A9CEC10C45F0625D26F66F7A5EB85D184C6CE9FE \
  --payload '' \
  > send1
```

Add it to the ledger:
```bash
./append_to_ledger --ledger-path ledger_data < send1
```

Create the corresponding receive on target account with previous as 0 to signify genesis transaction for target account:
```bash
./create_receive \
  --key 0000000000000000000000000000000000000000000000000000000000000002 \
  --previous 0000000000000000000000000000000000000000000000000000000000000000 \
  --send 043954BA0E0FFD6972E2410C443AE672B66DFB2DF72FF1B165AC5A21CABF0F5E \
  > receive1
```

Append to ledger:
```bash
./append_to_ledger --ledger-path ledger_data < receive1
```
