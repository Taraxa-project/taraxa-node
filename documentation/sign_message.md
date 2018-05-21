Signs a hex encoded message given on standard input with a key
given as argument and prints the signature to standard output:
```bash
$ echo deadbeef | ../sign_message --key 0000000000000000000000000000000000000000000000000000000000000001
3F26AFD8ADED36D94DFEAF2BB6BA82CBCA0935684D1B00B4A45DC01F9A1652B964DB68C02601E1EE061E3A12E18F8DB4EF1C378CA2C1ACC63D00A3D715195CCD
```
Use [verify_message](verify_message.md) to verify the produced signature.
