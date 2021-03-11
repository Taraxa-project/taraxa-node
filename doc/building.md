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
        ccache cmake gcc g++ clang-format clang-tidy cppcheck 
        libgflags-dev\
        libscrypt-dev \
        python3-pip

        
    # Install conan package manager
    pip3 install conan

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

Add bincrafters repository to conan:

    conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan

In general to build you need to:

    conan install -if build --build missing .
    conan build -bf build -sf . .

But you can do it that way too:

    conan install -if build --build missing .
    cd cmake-build
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j$(nproc) taraxad

And optional:

    # optional
    make install  # defaults to /usr/local

### Running tests

    conan install -if build --build missing .
    conan build -bf build -sf . .
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

Some known `brew` package dependencies (this list is not comprehensive):
```
coreutils, go, autoconf, automake, ccache, gflags,
gdb, git, libscrypt, libtool, makepkg-config, cmake
```

Also you need `conan` and follow build instructions above.

### Install taraxa-node dependencies:

    TODO: add


## Building using "taraxa-builder" docker image

TODO: tutorial How to use docker image for remote building will be added soon