# Basics

## Genesis transactions

Create a send transaction with some balance, without payload, self as receiver and previous as 0 to signify genesis transaction:
```bash
../create_send \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --new-balance 9 \
  --receiver 036B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296 \
  --previous 0000000000000000000000000000000000000000000000000000000000000000 \
  --payload '' \
  > genesis_transaction
```
You can use `echo 0000000000000000000000000000000000000000000000000000000000000001 | ../print_key_info` to find out the public key and the address of above key.


Add this transaction to the ledger located in ledger_data directory:
```bash
../append_to_ledger --ledger-path ledger_data < genesis_transaction
```

## Regular send/receive

Create a send of one coin, by decreasing balance of this account from 9 to 8, to another account with the hash of the above transaction as previous and the address of target account as receiver:
```bash
../create_send \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
  --new-balance 8 \
  --receiver 037CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978 \
  --previous 8EC16B869DEEA4E5F1823CDA8B9BFD5B5558AAB9DF9448AB9EFA0B02AEA70740 \
  --payload '' \
  > send1
```

Add it to the ledger:
```bash
../append_to_ledger --ledger-path ledger_data < send1
```

Create the corresponding receive on target account with previous as 0 to signify a first transaction for target account:
```bash
../create_receive \
  --key 0000000000000000000000000000000000000000000000000000000000000002 \
  --previous 0000000000000000000000000000000000000000000000000000000000000000 \
  --send 2ABDD47CDCBCAB56108DD9A4C14F238FD62DEC0B5B09FB921FFA8064E150E0FE \
  > receive1
```

Append to ledger:
```bash
../append_to_ledger --ledger-path ledger_data < receive1
```
