# taraxa-tools
Collection of tools for working with taraxa

Examples:

```bash
$ ./generate_private_key 
921D1F40235D5579FC487948CA1A4378CEA6AC8C

$ echo 921D1F40235D5579FC487948CA1A4378CEA6AC8C | ./print_key_info 
Private key:                921D1F40235D5579FC487948CA1A4378CEA6AC8C
Public key, first element:  5FFFEE57685F993B499E39FA3B1ADE0754047755
Public key, second element: E58B677AFEFD31D56B195314EC965095C16F7510

$ ./generate_private_key | ./print_key_info 
Private key:                53DE96C4D83A767D0BC2056F81BCE8F90C486E69
Public key, first element:  C4FDDA230FDF6705B3DDA00B16F4CE914D1C5842
Public key, second element: 4146A6940452D6BF6A658FB6E131A799D30969BD
```
