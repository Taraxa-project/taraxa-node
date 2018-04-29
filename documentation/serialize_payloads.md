# Payload serializer

Demonstration of payload serialization in the form of a Python "smart contract".

## Create token

Create contract and add it to ledger as account 036B:
```bash
./create_send \
    --key 0000000000000000000000000000000000000000000000000000000000000001 \
    --receiver 036B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296 \
    --new-balance 10 \
    --previous 0000000000000000000000000000000000000000000000000000000000000000 \
    --payload $(./bin2hex --input $'storage = dict()\nstorage[0x036B] = 10\n') \
| ./add_transaction --ledger-path ledger_data
```

## Transfer token

Transfer 1 token to another account.

Genesis to give balance to account 037C:
```bash
./create_send \
    --key 0000000000000000000000000000000000000000000000000000000000000002 \
    --receiver 037CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978 \
    --new-balance 10 \
    --previous 0000000000000000000000000000000000000000000000000000000000000000 \
    --payload '' \
| ./add_transaction --ledger-path ledger_data
```

Send transfer "interaction" with contract of 036B from account 037C:
```bash
./create_send \
    --key 0000000000000000000000000000000000000000000000000000000000000002 \
    --receiver 036B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296 \
    --new-balance 9 \
    --previous 213F422212267811E94DAFDD588479DA347D95D19568878205CF79D26803BBE0 \
    --payload $(./bin2hex --input $'storage[0x037C] = 0\nstorage[0x036B] -= 1\nstorage[0x037C] += 1\n') \
| ./add_transaction --ledger-path ledger_data
```

Receive from 036B:
```bash
./create_receive \
    --key 0000000000000000000000000000000000000000000000000000000000000001 \
    --previous 52FB9E72FF86B5B8667B942B9DC4E6651A0CD032C4FB01FE68F9FFE1B595620F \
    --send B65047F5969441EE0628D36672A2ADA47817C2EE9C2817AE0A6A1BD56C5EDE16 \
| ./add_transaction --ledger-path ledger_data
```

## Execute

Serialize payloads:
```bash
$ ./serialize_payloads --ledger-path ledger_data
storage = dict()
storage[0x036B] = 10
storage[0x037C] = 0
storage[0x036B] -= 1
storage[0x037C] += 1
```

Execute and print final persistent storage:
```bash
$ ./serialize_payloads --ledger-path ledger_data > ledger_data/serialized.py
$ echo 'print(storage)' >> ledger_data/serialized.py
$ python ledger_data/serialized.py
{875: 9, 892: 1}
```
