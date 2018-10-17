Verifies a hex encoded signature with signed message public key,
all given as arguments. Returns successfully if signature is
correct and unsuccessfully if not:
```bash
$ ../verify_message \
  --pubkey 036B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296 \
  --message deadbeef \
  --signature $(../sign_message --message deadbeef --key 0000000000000000000000000000000000000000000000000000000000000001)
$ echo $?
0
$ echo deadbeef ... --message deadbeef --signature ... --verbose
PASS
```
