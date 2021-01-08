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
        cmake ccache gcc g++ clang clang-format clang-tidy \
        libtool \
        autoconf \
        libssl-dev \
        libjsoncpp-dev libjsonrpccpp-dev \
        libscrypt-dev \
        librocksdb-dev \
        libmpfr-dev \
        libgmp3-dev
        
    # Boost packages (required)
    sudo apt-get install -y \
        libboost-program-options-dev \ 
        libboost-system-dev \
        libboost-filesystem-dev \ 
        libboost-thread-dev \
        libboost-log-dev \
        
    # Optional packages (not required, but will make a nicer experience)
    sudo apt-get install -y \
        doxygen

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

    mkdir build
    cd build
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
    cd build/src/taraxad

    # run taraxa-node
    ./taraxa-node --conf_taraxa /path/to/config/file

## Building on macOS X

Install Xcode and its command line tools by following the instructions here:
https://guide.macports.org/#installing.xcode. In OS X 10.11 (El Capitan)
and newer, you will be prompted to install developer tools when running a
developer command in the terminal.

Accept the Xcode license if you have not already:

    sudo xcodebuild -license accept

Install Homebrew by following the instructions here: http://brew.sh/

### Initialize Homebrew:

    brew doctor
    brew update

### Install taraxa-node dependencies:

    TODO: add

### Clone the Repository

    git clone https://github.com/Taraxa-project/taraxa-node.git
    cd taraxa-node

### Compile

    TODO: add

### Running tests

    TODO: add

### Running taraxa-node

    TODO: add

## Building using "taraxa-builder" docker image

TODO: tutorial How to use docker image for remote building will be added soon