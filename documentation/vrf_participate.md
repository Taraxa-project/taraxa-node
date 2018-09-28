Prints the hash of proof of public key hash of given message
with given private key:
```bash
$ ../vrf_participate \
    --key 0000000000000000000000000000000000000000000000000000000000000001 \
    --message deadbeef
VRF output for message DEADBEEF: A79D6E18EB4FA95787AAFDE0A28EFCE62D58DF8FBE730A69E04F1EB6D58066EB
```
Winning key for given message can be selected "randomly" e.g. as
lowest printed hash of its proof.
