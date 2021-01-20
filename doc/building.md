# Building taraxa-node

### Compile-Time Options (cmake)

#### CMAKE_BUILD_TYPE=[Release/Debug]

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
        libboost-program-options-dev libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-log-dev \
        libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev \
        libssl-dev \
        libjsoncpp-dev libjsonrpccpp-dev \
        libscrypt-dev \
        libmpfr-dev \
        libgmp3-dev

        
    # Optional packages (not required, but will make a nicer experience)
    sudo apt-get install -y \
        doxygen

    # rocksdb (required)
    curl -SL https://github.com/facebook/rocksdb/archive/v5.18.3.tar.gz \
        | tar -xzC /tmp \
    && cd /tmp/rocksdb-5.18.3 \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' PORTABLE=1 make -j $(nproc) install-static \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' PORTABLE=1 make -j $(nproc) install-shared \
    && rm -rf $(pwd) \
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

    # Go (required)
    wget https://dl.google.com/go/go1.13.7.linux-amd64.tar.gz
    tar -xf go1.13.7.linux-amd64.tar.gz -C /usr/local
    rm -rf go1.13.7.linux-amd64.tar.gz

    # Add go to PATH
    # Add these env. variables to the ~/.profile to persist go settings even after restart
    export GOROOT=/usr/local/go
    export GOPATH=$HOME/.go
    export PATH=$GOPATH/bin:$GOROOT/bin:$PATH

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git
    cd taraxa-node
    git checkout master
    git submodule update --init --recursive

### Compile

    mkdir cmake-build
    cd cmake-build
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j$(nproc) taraxad
    
    # optional
    make install  # defaults to /usr/local

### Running tests
    make -j$(nproc) all
    cd tests

    # run tests
    ctest

### Running taraxa-node
    cd cmake-build/src/taraxad

    # run taraxa-node
    ./taraxad --conf_taraxa /path/to/config/file

    e.g.:
    ./taraxad --conf_taraxa configs/taraxad.conf

## Building on MacOS

Refer to the [MacOS dev doc](../for_devs_on_macos/README.md)

### Install taraxa-node dependencies:

    TODO: add


## Building using "taraxa-builder" docker image

TODO: tutorial How to use docker image for remote building will be added soon