FROM ubuntu:18.10 as builder

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm

# fixme: we have multiple openssl
RUN apt-get update \
    && apt-get install -y \
    libgflags-dev libsnappy-dev zlib1g-dev libicu-dev libbz2-dev libzstd-dev liblz4-dev gcc-8 g++-8 clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential xz-utils curl libcurl4-openssl-dev cmake unzip pkg-config git \
    rapidjson-dev python-dev libxml2-dev libxslt-dev libscrypt-dev libssl-dev openssl libgmp3-dev autoconf libtool \
    libjsoncpp-dev libjsonrpccpp-dev libjsonrpccpp-tools libmpfr-dev

# After install and link for json/json.h include dir
RUN ln -s /usr/include/jsoncpp/json /usr/include/json


RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

# Add multi-stage build for docker layer caching
FROM builder as boost-layer

ENV BOOST_VERSION="1_71_0"
RUN cd /tmp \
    && wget https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.bz2 \
    && tar --bzip2 -xf boost_${BOOST_VERSION}.tar.bz2 \
    && export CPLUS_INCLUDE_PATH="$CPLUS_INCLUDE_PATH:/usr/include/python2.7/"
RUN cd /tmp/boost_${BOOST_VERSION} \
    && ls -al \
    && ./bootstrap.sh --prefix=/usr/local
RUN cd /tmp/boost_${BOOST_VERSION}; ls -al ; ./b2 install
RUN cd /tmp/boost_${BOOST_VERSION} \
    && ln -s /usr/local/lib/libboost_thread.so /usr/local/lib/libboost_thread-mt.so

FROM boost-layer as gtest-layer
ENV LD_LIBRARY_PATH /usr/local/lib/
RUN git clone https://github.com/google/googletest /tmp/gtest \
  && cd /tmp/gtest && mkdir build && cd build && cmake .. \
  && make -j $(nproc) && make -j $(nproc) install

FROM gtest-layer as rocksdb-layer
ARG rocksdb_version=5.18.3
ENV ROCKSDB_VERSION="$rocksdb_version"
RUN wget https://github.com/facebook/rocksdb/archive/v$rocksdb_version.zip \
    && unzip v$rocksdb_version.zip -d /tmp \
    && cd /tmp/rocksdb-$rocksdb_version \
    && make -j $(nproc) shared_lib PORTABLE=1 \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include

FROM rocksdb-layer as leveldb-layer
WORKDIR /tmp/
ENV LEVELDB_VERSION="1.20"
RUN wget https://github.com/google/leveldb/archive/v${LEVELDB_VERSION}.tar.gz \
  && tar xvf v${LEVELDB_VERSION}.tar.gz \
  && rm -f v${LEVELDB_VERSION}.tar.gz

WORKDIR /tmp/leveldb-${LEVELDB_VERSION}
RUN make -j $(nproc)
RUN scp -r out-static/lib* out-shared/lib* "/usr/local/lib"
RUN scp -r include/leveldb /usr/local/include
RUN ldconfig

FROM leveldb-layer as go-layer
ARG go_version=1.13
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
    OPENSSL_HOME=${APP_LIBRARY_HOME} -j $(nproc) dependencies