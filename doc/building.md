# Building taraxa-node

### Compile-Time Options (cmake)

#### CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]

Specifies whether to build with or without optimization and without or with the symbol table for debugging. Unless you are specifically debugging or running tests, it is recommended to build as release.

## Building project 

### 1. Install dependencies 

#### [Ubuntu 24.04]
    # Required packages
    sudo apt-get install -y \
        libtool \
        autoconf \
        ccache \
        cmake \
        clang \
        clang-format-18 \
        clang-tidy-18 \
        llvm-18 \
        golang-go \
        python3-full \
        libzstd-dev \
        libsnappy-dev \
        libmicrohttpd-dev

    # Optional. Needed to run py_test. This won't install on arm64 OS because package is missing in apt
    sudo add-apt-repository ppa:ethereum/ethereum
    sudo apt-get update
    sudo apt install solc

    # Install conan package manager
    sudo python3 -m pip install conan==2.15.0

#### [MacOS]

First you need to get [Homebrew](https://brew.sh/) package manager. After that you need to install dependencies with it. Clang-18 is used for compilation.

    brew update
    brew install coreutils go autoconf automake gflags git libtool llvm@18 make pkg-config cmake conan snappy zstd libmicrohttpd

### 2. Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git
    cd taraxa-node
    git submodule update --init --recursive

### 3. Compile

    # Build project
    ./scripts/build.sh


### Known issues

#### Issues with conan cache

Sometimes conan cache goes wrong, so you should clean it up. You could face error like:
```
ERROR: boost/1.76.0: Error in package_info() method, line 1492
    raise ConanException("These libraries were expected to be built, but were not built: {}".format(non_built))
    ConanException: These libraries were expected to be built, but were not built: {'boost_math_c99l', 'boost_json', 'boost_math_c99', 'boost_nowide', 'boost_math_tr1l', 'boost_math_tr1f', 'boost_math_tr1', 'boost_math_c99f'}
```

It could be cleaned up with:

```
conan cache clean
```


## Run
### Running tests

    cd build
    make all_tests

    or

    cd build/tests
    ctest

### Running taraxa-node

    cd build/bin

Run taraxa node with default testnet config which on initial run will generate default
config and wallet file in `~/.taraxa/config.json` and `~/.taraxa/wallet.json`

    # run taraxa-node
    ./taraxad

Run taraxa node with specified config and wallet files

    # run taraxa-node
    ./taraxad --config /path/to/config/file --wallet /path/to/wallet/file

Run help message to display all command line options to run and configure node
in devnet, testnet or custom network

    # help
    ./taraxad --help

