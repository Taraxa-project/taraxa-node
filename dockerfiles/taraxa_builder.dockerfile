# Taraxa remote docker environment - it is intended to be used for taraxa-node building
#
# Build and run:
#
#   DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa/taraxa_builder:1.0 -f taraxa_builder.dockerfile .
#   docker run -ti --name taraxa_builder taraxa/taraxa_builder:1.0 /bin/bash
#
# stop:
#   docker stop taraxa_builder
#
# users:
#   root

FROM ubuntu:20.04

# Install standard packages
# TODO: add rocksdb install deps from https://github.com/Level/rocksdb/blob/master/deps/leveldb/leveldb-rocksdb/INSTALL.md
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata \
    && apt-get install -y \
        tar \
        curl \
        cmake ccache gcc g++ clang-format clang-tidy cppcheck \
        libboost-program-options-dev libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-log-dev \
        libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev \
        libtool \
        autoconf \
        libssl-dev \
        libjsoncpp-dev libjsonrpccpp-dev \
        libscrypt-dev \
        libmpfr-dev \
        libgmp3-dev \
        librocksdb-dev \
    && rm -rf /var/lib/apt/lists/*

# TODO: when static linking of rocksdb is done, use manual illation instead of ubuntu package
# # Install rocksdb
# ARG ROCKSDB_VERSION=5.18.3
# RUN curl -SL https://github.com/facebook/rocksdb/archive/v$ROCKSDB_VERSION.tar.gz \
#     | tar -xzC /tmp \
#     && cd /tmp/rocksdb-${ROCKSDB_VERSION} \
#     && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' make -j $(nproc) install-shared \
#     && rm -rf $(pwd)

# Install go
ARG GO_VERSION=1.13.7
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
        | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
