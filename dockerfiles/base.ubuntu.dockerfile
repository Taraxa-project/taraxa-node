FROM ubuntu:20.04 as builder

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm

# fixme: we have multiple openssl
RUN apt-get update \
    && apt-get install -y \
    libgflags-dev zlib1g-dev libicu-dev libbz2-dev libzstd-dev liblz4-dev gcc-8 g++-8 clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential xz-utils curl libcurl4-openssl-dev unzip pkg-config git \
    python2-dev libxml2-dev libxslt-dev libscrypt-dev libssl-dev openssl libgmp3-dev autoconf libtool \
    libjsoncpp-dev libjsonrpccpp-dev libjsonrpccpp-tools libmpfr-dev python3-pip python3-dev

RUN pip3 install --upgrade pip
RUN pip3 install cmake==3.17.2

# After install and link for json/json.h include dir
RUN ln -s /usr/include/jsoncpp/json /usr/include/json


RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

# Add multi-stage build for docker layer caching
FROM builder as rocksdb-layer
ARG rocksdb_version=5.18.3
ENV ROCKSDB_VERSION="$rocksdb_version"
RUN wget https://github.com/facebook/rocksdb/archive/v$rocksdb_version.zip \
    && unzip v$rocksdb_version.zip -d /tmp \
    && cd /tmp/rocksdb-$rocksdb_version \
    && make -j $(nproc) shared_lib PORTABLE=1 \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include

FROM rocksdb-layer as go-layer
ARG go_version=1.13.7
RUN wget -qO- --show-progress --progress=bar:force \
    https://dl.google.com/go/go$go_version.linux-amd64.tar.gz \
    | tar xvz -C /usr/local
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH

FROM go-layer as release
# Clean up image
WORKDIR /app
RUN cd / && rm -rf /tmp/*

ENV APP_PATH /opt/taraxa/taraxa-node
ENV APP_LIBRARY_HOME /usr/local
ENV LD_LIBRARY_PATH ${APP_LIBRARY_HOME}/lib
WORKDIR ${APP_PATH}

COPY . .
RUN make -f Makefile.submodules \
    DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) submodules
