# Building taraxa-node

### Compile-Time Options (cmake)

#### CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]

Specifies whether to build with or without optimization and without or with
the symbol table for debugging. Unless you are specifically debugging or
running tests, it is recommended to build as release.

## Building on Ubuntu 24.04
For Ubuntu 24.04 users, after installing the right packages with `apt` taraxa-node
will build out of the box without further effort:

### Install taraxa-node dependencies:

    # Required packages
    sudo apt-get install -y \
        libtool \
        autoconf \
        ccache \
        cmake \
        clang-format-17 \
        clang-tidy-17 \
        llvm-17 \
        golang-go \
        python3-full \
        # this libs are required for arm build by go part. you can skip it for amd64 build
        libzstd-dev \
        libsnappy-dev \
        rapidjson-dev \
        libgmp-dev \
        libmpfr-dev \
        libmicrohttpd-dev

    # Optional. Needed to run py_test. This won't install on arm64 OS because package is missing in apt
    sudo add-apt-repository ppa:ethereum/ethereum
    sudo apt-get update
    sudo apt install solc

    # Install conan package manager
    sudo python3 -m pip install conan==1.64.1

    # Setup clang as default compiler either in your IDE or by env. variables"
    export CC="clang-17"
    export CXX="clang++-17"

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git --branch testnet
    cd taraxa-node
    git submodule update --init --recursive

### Compile

    # Optional - one time action
    # Create clang profile
    # It is recommended to use clang because on other compilers you could face some errors
    conan profile new clang --detect \
    && conan profile update settings.compiler=clang clang  \
    && conan profile update settings.compiler.version=17 clang  \
    && conan profile update settings.compiler.libcxx=libstdc++11 clang \
    && conan profile update settings.build_type=RelWithDebInfo clang \
    && conan profile update env.CC=clang-17 clang  \
    && conan profile update env.CXX=clang++-17 clang  \
    && conan install --build missing -pr=clang .

    # Compile project using cmake
    mkdir cmake-build
    cd cmake-build
    cmake -DCONAN_PROFILE=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTARAXA_ENABLE_LTO=OFF -DTARAXA_STATIC_BUILD=OFF ../
    make -j$(nproc)

## Building on MacOS

### Install taraxa-node dependencies:

First you need to get (Brew)[https://brew.sh/] package manager. After that you need tot install dependencies with it. Clang-17 is used for compilation.

    brew update
    brew install coreutils go autoconf automake gflags git libtool llvm@17 make pkg-config cmake conan snappy zstd rapidjson gmp mpfr libmicrohttpd

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git --branch testnet
    cd taraxa-node
    git submodule update --init --recursive

### Compile

    # Optional - one time action
    # It is recommended to use clang because on other compilers you could face some errors
    conan profile new clang --detect && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=17 clang && \
    conan profile update settings.compiler.compiler.cppstd=17
    conan profile update settings.compiler.libcxx=libc++ clang && \
    conan profile update env.CC=clang clang && \
    conan profile update env.CXX=clang++ clang

    # Export needed var for conan
    export CONAN_REVISIONS_ENABLED=1

    # Build project
    mkdir cmake-build
    cd cmake-build
    cmake -DCONAN_PROFILE=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
    make -j$(nproc)

And optional:

    # optional
    make install  # defaults to /usr/local

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
rm -rf ~/.conan/data
```

## Building on M1 Macs for x86_64 with Rosetta2

You should be able to build project following default MacOS building process. But here is a guide how to build project for x86_64 arch with Rosetta2.

### Install Rosetta2

    softwareupdate --install-rosetta

### Run an x86_64 session

    arch -x86_64 zsh

### Install Homebrew

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

### Install dependencies

    /usr/local/bin/brew install coreutils go autoconf automake gflags git libtool llvm@13 make pkg-config cmake conan snappy zstd rapidjson gmp mpfr libmicrohttpd

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git --branch testnet
    cd taraxa-node
    git submodule update --init --recursive

### Compile

    # Optional - one time action
    # It is recommended to use clang because on other compilers you could face some errors
    # Make sure that you executing this from x86_64 CLI. Could be verified with `arch` command
    # It output should be equal to `i386`
    conan profile new clang --detect && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=17 clang && \
    conan profile update settings.compiler.libcxx=libc++ clang && \
    conan profile update env.CC=/usr/local/opt/llvm/bin/clang clang && \
    conan profile update env.CXX=/usr/local/opt/llvm/bin/clang++ clang

    # Export needed var for conan
    export CONAN_REVISIONS_ENABLED=1

    # Build project
    mkdir cmake-build
    cd cmake-build
    # Make sure that you are specifying the full path to C and CXX compilers
    cmake -DCONAN_PROFILE=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ ../
    make -j$(nproc)

## Run
### Running tests

> OSX: maybe you need to set a new limit for max open files per thread/process: `ulimit -n 200000`

    cd cmake-build-release
    make all_tests

    or

    cd cd cmake-build-release/tests
    ctest

### Running taraxa-node
    cd cmake-build-release/src/taraxad

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

## Building using "taraxa-builder" docker image

TODO: tutorial How to use docker image for remote building will be added soon
