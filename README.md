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
**The compilation requires Go 1.11+ and a C++17-compatible compiler**

## Library dependency:

```
cryptopp, libff, ethash, secp256k1, boost, rocksdb, rapidjson, gtest, leveldb
```

**On a Macintosh** Installing boost, rocksdb, rapidjson, leveldb ... using brew:
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
brew install pkg-config
brew install jsoncpp
brew install libjson-rpc-cpp
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

## Demo prometheus-cpp:
/* To demo the prometheus cpp locally, follow the steps below:
 * ---------------------------  pushgateway  ---------------------------
 * Step 1: download pushgateway binary from https://prometheus.io/download/#pushgateway
 * Step 2: run push gateway at localhost, set port 9091 using: './pushgateway --web.listen-address=0.0.0.0:9091'
 * Step 3: open a browser and access localhost:9091/
 * ---------------------------  prometheus itself  ---------------------------
 * // TODO: current promethes was not connected with the push-gateway
 * Step 4: download prometheus from https://prometheus.io/download/ ; the tar file contains an executable 'prometheus'
 * Step 5: config 'prometheus.yml' (more info: https://prometheus.io/docs/prometheus/latest/configuration/configuration/)
 * Step 6: start the prometheus with './prometheus --config.file=prometheus.yml'
 * ---------------------------  prometheus cpp client:  ---------------------------
 * The prometheus cpp is expected to be built through MakeFile's git submodule (`git submodule update`). 
 
 Run instructions below if makefile fails
 * Step 7: mkdir -p submodules/prometheus-cpp/
 * Step 8: in the directory, git clone https://github.com/jupp0r/prometheus-cpp.git
 * Step 9: build prometheus_cpp client following the Building section and 'via CMake' sub-section at
 *         https://github.com/jupp0r/prometheus-cpp
 */

```
make pdemo
# replace all of the following param for a different pushgateway setup
# make pdemo PUSHGATEWAY_NAME=pushgateway PUSHGATEWAY_IP=0.0.0.0 PUSHGATEWAY_PORT=9091
```

Google how to install other libs for your system.

# Run taraxa-node tests:
Tests are under core_tests/
```
make run_test
```
# Run taraxa full node
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

## Build debug executable:

```
make main DEBUG=1
```

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

## CPU Profiling 
Currently, use https://github.com/gperftools/gperftools to run CPU profiling. It works in Linux environment but **NOT** in latest MacOs (Mojave). 

For example, install in Ubuntu:
```sudo apt-get install google-perftools libgoogle-perftools-dev```

Build with `PERF=1`, for example: `make run_test PERF=1`

To do profiling in a docker, do: 
```
docker build -t taraxa-node:builder -f dockerfiles/Dockerfile --target builder .
```
Login to docker: 
```
docker run -it <image> /bin/bash
```

