# Taraxa Full Node

# Running the container image

Running locally using docker (This executes the Taraxa CLI help menu)

```bash
$ docker run --name taraxa-node taraxa/taraxa-node:latest
```

### Generating config files

Use the included CLI to generate a config file. This should be written to a mounted volume so that it persists. (Mounting volumes is not documented here.)

There are many options for generating a config file. See the help menu for updated options.

```bash
$ docker run taraxa/taraxa-node:latest config --help
```

In most cases, you will want to join an existing network using an existing chain config, so you need a node config.

```bash
# generate config to join testnet
$ docker run taraxa/taraxa-node:latest config node \
  --network testnet \
  --file ./path/to/node/config.json
```

### Running with existing config files

To start a node using an existing node config and chain config, pass the following parameters to the start command:

```bash
$ docker run taraxa/taraxa-node:latest start \
  --conf_taraxa ./path/to/node/config.json \
  --conf_chain ./config/chains/testnet.json
```

### Doing both: Keeping config updated when changing versions

The CLI will read from the template and update the config while preserving any important values (private keys, etc) from your previous config.

```bash
docker run --entrypoint /bin/sh taraxa:taraxa-node:latest \
  -c "./taraxa config node --network testnet --file ./path/to/file && ./taraxa start --conf_taraxa ./path/to/file
```

## Development

### Submodule workflow
Keep in mind that `make` build will update submodules automatically to their 
*committed* versions, also cleaning any untracked files if a the current version
is is different from the committed for a submodule. This should be convenient 
in most cases, but if you are developing in a submodule and want to 
incrementally integrate those changes in this repo, you should not forget 
to *commit* the submodule directory after updating it from the remote.
The best way to update the local submodule version - is to enter it's directory
and do git checkout/pull for the branch of interest, or just checkout if there's
no branch of interest.

### Keeping development branches updated
Please rebase your `feature` branch **_everyday_** and **_before_** create pull request.

(Or at least before create pull request). 

In your `feature` branch, do:
```
  git fetch origin
  git rebase origin/master
```
This makes sure your codebase is up-to-date. 
Please resolve any conflicts you may find.  :)

Before filing pull request, please do:

`make run_test` 

and make sure all test are passed! :)

Please run your code format with `.clang-format` checking. 

### Installation:
```
git clone https://github.com/Taraxa-project/taraxa-node.git
```
**The compilation requires Go 1.12+ and a C++17-compatible compiler**

### Library dependency:

```
cryptopp, libff, ethash, secp256k1, boost, rocksdb, gtest, leveldb
```

**On MacOS**:
```
brew install boost
brew install rocksdb
brew install cmake
brew install leveldb
brew install libscrypt
brew install automake
brew install jsoncpp
brew install libjson-rpc-cpp
```

**On Linux**:
[Refer to the ubuntu env dockerfile 
to find what dependecies have to be installed](dockerfiles/base.ubuntu.dockerfile)

Some libraries are managed as git submodules, 
and will be installed automatically during the build


```
make pdemo
# replace all of the following param for a different pushgateway setup
# make pdemo PUSHGATEWAY_NAME=pushgateway PUSHGATEWAY_IP=0.0.0.0 PUSHGATEWAY_PORT=9091
```

Google how to install other libs for your system.

### Run taraxa-node tests:
Tests are under core_tests/
```
make run_test
```
### Run taraxa full node
To run full node requires one configuration file (in JSON format). 

```
make main
```
use `./build/main --help` to check options.

Run example:
```
./build/main --conf_taraxa ./core_tests/conf_taraxa1.json -v 1 --log-filename mylog --log-channels FULLND
```
The example run taraxa node with _conf_taraxa1.json_ setting. Reports error and warning message (verbosity 1) in log channel _FULLND_. It also pipe message to _mylog_ file.

Please change ip address, binding port, number of threads, etc, in the configuration files accordingly.

Verbosity setting (-v):

0: Error, 1: Warning (default), 2: Info, 3: Debug, 4: Trace 

### Build debug executable:

```
make main DEBUG=1
```

### Building Docker Image

```bash
$ docker build -t taraxa-node -f dockerfiles/main.ubuntu.dockerfile .
```

*The building process takes a lot of time, especially during its first execution.*


## CPU Profiling 
Currently, use https://github.com/gperftools/gperftools to run CPU profiling. It works in Linux environment but **NOT** in latest MacOs (Mojave). 

For example, install in Ubuntu:
```sudo apt-get install google-perftools libgoogle-perftools-dev```

Build with `PERF=1`, for example: `make run_test PERF=1`