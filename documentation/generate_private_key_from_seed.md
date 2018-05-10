This program generates private keys from seed given
as argument and prints them to standard output.

You can pipe the output to print_key_info to see all
relevant information about generated private keys:
```bash
$ for key in $( \
  ../generate_private_key_from_seed \
  --seed 000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 \
  --last 2 \
); do echo $key | ../print_key_info; done
Private key: DC95C078A2408989AD48A2149284208708C374848C228233C2B34F332BD2E9D3
Public key:  03BC2392E4219896D052A18D764AA0D36EDC8E6F65A5AB183FB0F5FEBFB8909FE4
Private key: 8B70C515A6663D38CDB8E6532B2664915D0DCC192580AEE9EF8A8568193F1B44
Public key:  03A1F2B4BC8E3FBC4378DD7362D41A28C2247F53E169210549431F75323B15F9DA
Private key: BFCA557C6BAB7DC79DA07FFD42191B263FEA78D11CD6C932B79BBCF4343C8516
Public key:  028C1D9FEC67E819D020325BFC027BD4BA35CD835BEDC132AAF95710BEB7B53479
```
Probably secure way to generate seeds is to run `generate_private_key`
twice and use first key and half of second as seed.
