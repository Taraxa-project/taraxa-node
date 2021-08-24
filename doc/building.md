# Building taraxa-node

### Compile-Time Options (cmake)

#### CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]

Specifies whether to build with or without optimization and without or with
the symbol table for debugging. Unless you are specifically debugging or
running tests, it is recommended to build as release.

## Building on Ubuntu 20.04
For Ubuntu 20.04 users, after installing the right packages with `apt` taraxa-node
will build out of the box without further effort:

### Install taraxa-node dependencies:

    # Required packages
    sudo apt-get install -y \
        libtool \
        autoconf \
        ccache cmake gcc g++ clang-format clang-tidy cppcheck \
        libgflags-dev\
        libjsoncpp-dev \
        libjsonrpccpp-dev \
        python3-pip


    # Install conan package manager
    # >= 1.36.0 version is required to work properly with clang-12
    sudo python3 -m pip install conan

    # Go (required)
    wget https://dl.google.com/go/go1.16.3.linux-amd64.tar.gz
    tar -xf go1.16.3.linux-amd64.tar.gz -C /usr/local
    rm -rf go1.16.3.linux-amd64.tar.gz

    # Add go to PATH
    # Add these env. variables to the ~/.profile to persist go settings even after restart
    export GOROOT=/usr/local/go
    export GOPATH=$HOME/.go
    export PATH=$GOPATH/bin:$GOROOT/bin:$PATH

    # Optional
    # We are using clang from llvm toolchain as default compiler as well as clang-format and clang-tidy
    # It is possible to build taraxa-node also with other C++ compilers but to contribute to the official repo,
    # changes must pass clang-format/clang-tidy checks for which we internally use llvm version=13
    # To install llvm:
    sudo su

    curl -SL -o llvm.sh https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 12 && \
    apt-get install -y clang-format-12 clang-tidy-12 && \
    rm -f llvm.sh

    # Setup clang as default compiler either in your IDE or by env. variables"
    export C="clang-12"
    export CXX="clang++-12"

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git
    cd taraxa-node
    git checkout master
    git submodule update --init --recursive

### Compile

    # Optional - one time action
    # Create clang profile (we are using clang in taraxa, but any C++ compiler can be used)
    conan profile new clang --detect && \
    conan profile update settings.compiler=clang clang && \
    conan profile update settings.compiler.version=12 clang && \
    conan profile update settings.compiler.libcxx=libstdc++11 clang && \
    conan profile update env.CC=clang-12 clang && \
    conan profile update env.CXX=clang++-12 clang

    # Export needed var for conan
    export CONAN_REVISIONS_ENABLED=1

    # Fetch and compile libraries fetched from conan
    conan remote add -f bincrafters "https://bincrafters.jfrog.io/artifactory/api/conan/public-conan" && \
    conan install --build missing -s -pr=clang .

    # Compile project using cmake
    mkdir cmake-build-release
    cd cmake-build-release
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j$(nproc) taraxad

And optional:

    # optional
    make install  # defaults to /usr/local

## Building on MacOS

### Install taraxa-node dependencies:

First you need to get (Brew)[https://brew.sh/] package manager. After that you need tot install dependencies with it:

    brew update
    brew install coreutils go autoconf automake gflags git libtool make pkg-config cmake conan

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git
    cd taraxa-node
    git checkout master
    git submodule update --init --recursive

### Compile

    # Export needed var for conan
    export CONAN_REVISIONS_ENABLED=1
    # Add bincrafters remote
    conan remote add -f bincrafters "https://bincrafters.jfrog.io/artifactory/api/conan/public-conan"

    # Two build options
        1. Compile project using conan
        conan install -if ../conan-build --build missing -s build_type=Debug .
        conan build -bf ../conan-build -sf . .

        2.Compile project using cmake
        mkdir cmake-build-release
        cd cmake-build-release
        cmake -DCMAKE_BUILD_TYPE=Release ../
        make -j$(nproc) taraxad

And optional:

    # optional
    make install  # defaults to /usr/local

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
