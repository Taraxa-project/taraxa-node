# Current Build Status
[![Build Status](https://api.travis-ci.com/Taraxa-project/taraxa-node.svg?token=uj8tJYwJxx7PyPVNRSWp&branch=master)]([https://travis-ci.org/Taraxa-project/taraxa-node](https://travis-ci.com/Taraxa-project/taraxa-node))

# IMPORTANT!!!   *** Developer Notice ***
Please rebase your `feature` branch **_everyday_** and **_before_** create pull request.

(Or at lease before create pull request). 

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

# Installation:
```
git clone https://github.com/Taraxa-project/taraxa-node.git --recursive
```

## Library dependency:

```
cryptopp, libff, ethash, secp256k1, boost, rocksdb, rapidjson, gtest, leveldb
```

**On a Macintosh** Installing boost, rocksdb, rapidjson, leveldb, grpc ... using brew:
```
brew install boost
brew install rocksdb
brew install rapidjson
brew install gmp
brew install cmake
brew install openssl
brew install leveldb
brew install libscrypt
brew install automake
brew install libtool
brew install grpc
brew install pkg-config
```

Libraries cryptopp, libff, ethash and secp256k1 are in the submodules directory. These libraries should be built automatically. In case there is an issue see README files in each submodule on how to build them.

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

## Protobuf and gRPC and google_apis 
The project uses gRPC. 

If proto file changed, need to rebuid using: 

`make protoc_taraxa_grpc`

(Not needed, can skip for now) For google API, the C++ library for googleapis is unavailable. 
Therefore, googleapis header and source files are included in `google` directory.
To Build googleapis static libs:
```
cd google
make
```

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

## Docker

Docker has to be installed. If it is not, visit this [web](https://docs.docker.com/install/).

### Building Docker Image

```bash
$ docker build -t taraxa-node -f dockerfiles/Dockerfile .
```

*The building process takes a lot of time, especially during its first execution.*

### Check Docker images

```bash
$ docker images
```

### Running a Docker Container

* Running using the host network mode

```bash
$ docker run --rm --net=host --name taraxa-node taraxa-node
```

* Running using the bridge network mode

```bash
$ docker run --rm -p <host port>:<container port> \
  --name taraxa-node taraxa-node
```

Example:

```bash
$ docker run --rm -p 7777:7777 \
  --name taraxa-node taraxa-node
```

* Running with my own config files

```bash
$ docker run --rm -p <host port>:<container> -v `pwd`/core_tests:/core_tests \
  --name taraxa-node taraxa-node \
  --conf_full_node /core_tests/conf_full_node1.json \
  --conf_network /core_tests/conf_network1.json \
  --conf_rpc /core_tests/conf_rpc1.json
```

Example:

```bash
$ docker run --rm -p 7777:7777 -v `pwd`/core_tests:/core_tests \
  --name taraxa-node taraxa-node \
  --conf_full_node /core_tests/conf_full_node1.json \
  --conf_network /core_tests/conf_network1.json \
  --conf_rpc /core_tests/conf_rpc1.json
```

### Check Running Docker containers

```bash
$ docker ps
```

### Check Stopped Docker containers

```bash
$ docker ps -a
```

### Stop/Kill Docker Container

```bash 
$ docker stop taraxa-node
```

```bash 
$ docker kill taraxa-node
```

### Remove Stopped Docker Container

```bash 
$ docker rm taraxa-node
```

## Base Image for Taraxa Node

### Build Image

```bash
$ docker build -t taraxa-node-base -f dockerfiles/base.<chosen base image>.dockerfile .
```

### Tagging Image
```bash
$ docker tag taraxa-node-base <account id>.dkr.ecr.<aws region>.amazonaws.com/taraxa-node-base
```

### Push Image

```bash
$ docker push <account id>.dkr.ecr.<aws region>.amazonaws.com/taraxa-node-base
```

> Log into the registry is required.

### Pull Image

```bash
$ docker pull <account id>.dkr.ecr.<aws region>.amazonaws.com/taraxa-node-base
```

> Log into the registry is required.
