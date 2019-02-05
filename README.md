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
# Run taraxa full node
To run full node requires three files for full node, network and rpc configurations (in JSON format). 

```
make main
```
use `./build/main --help` to check options.

Run example:
```
./build/main --conf_full_node ./core_tests/conf_full_node1.json --conf_network ./core_tests/conf_network1.json --conf_rpc ./core_tests/conf_rpc1.json
```
Please change ip address, binding port, number of threads, etc, in the configuration files accordingly.
