# Default output dir
ARG OUTPUT_DIR=output

FROM ubuntu:20.04 as deps

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
        automake\
        libssl-dev \
        libjsoncpp-dev libjsonrpccpp-dev \
        libscrypt-dev \
        libmpfr-dev \
        libgmp3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install rocksdb
RUN curl -SL https://github.com/facebook/rocksdb/archive/v5.18.3.tar.gz \
    | tar -xzC /tmp \
    && cd /tmp/rocksdb-5.18.3 \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' make -j $(nproc) install-static \
    && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' make -j $(nproc) install-shared \
    && rm -rf $(pwd)
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

# Install go
ARG GO_VERSION=1.13.7
RUN curl -SL https://dl.google.com/go/go$GO_VERSION.linux-amd64.tar.gz \
        | tar -xzC /usr/local

# Add go to PATH
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH


# Builder image
FROM deps as builder

ARG OUTPUT_DIR
ENV INSTALL_DIR=install

WORKDIR /opt/taraxa/
COPY . .
RUN mkdir $OUTPUT_DIR \
    && cd $OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=Release \
             -DTARAXA_STATIC_BUILD=ON \
             -DTARAXAD_INSTALL_DIR=./install \
             -DTARAXAD_CONF_INSTALL_DIR=./install \
             ../ \
    && make -j$(nproc) all \
    && make install \
    && cd tests/ \
    && ctest


# Taraxa image
FROM ubuntu:20.04

ARG OUTPUT_DIR
WORKDIR /opt/taraxa/taraxa-node

# Keep the old struct for now
COPY --from=builder /opt/taraxa/$OUTPUT_DIR/src/taraxad/taraxad ./main
COPY tests/util_test/conf/conf_taraxa1.json ./default_config/

ENV GODEBUG cgocheck=0
ENTRYPOINT ["./main"]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
