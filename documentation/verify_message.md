Verifies a hex encoded signature given on standard input
with signed message given as argument against public key
given as argument. Returns successfully if signature is
correct and unsuccessfully if not:
```bash
$ echo deadbeef \
| ../sign_message \
  --key 0000000000000000000000000000000000000000000000000000000000000001 \
| ../verify_message \
  --pubkey 036B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296 \
  --message deadbeef
$ echo $?
0
$ echo deadbeef ... --message deadbeef --verbose
PASS
```
