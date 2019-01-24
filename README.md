# Installation:
```
git clone https://github.com/Taraxa-project/taraxa-node.git --recursive --recursive
```

## Library dependency:

```
cryptopp, boost, rocksdb, rapidjson, gtest
```

Build cryptopp:
```
cd submodules/cryptopp; make;
```

**On a Macintosh** Installing boost, rocksdb, rapidjson using brew:
```
brew install boost
brew install rocksdb
brew install rapidjson
```


Install gtest:
```
git clone https://github.com/google/googletest
cd googletest
mkdir build
cd build
cmake ..
make
make install
```

Google how to install other libs for your system.

# Run taraxa-node tests:
Tests are under core_tests/
```
make run_test
```
