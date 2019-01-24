# Installation:
```
git clone git@github.com:Taraxa-project/taraxa-node.git --recursive
```

## Library dependency:

```
cryptopp, boost, rocksdb, rapidjson, gtest
```

Build cryptopp:
```
cd submodules/cryptopp; make;
```
Google how to install other libs for your system.

# Run taraxa-node tests:
Tests are under core_tests/
```
make run_test
```
