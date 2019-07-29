FROM ubuntu:18.10 as builder

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm

RUN apt-get update \
    && apt-get install -y \
    libgflags-dev libsnappy-dev zlib1g-dev libicu-dev libbz2-dev libzstd-dev liblz4-dev gcc-8 g++-8 clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential xz-utils curl libcurl4-openssl-dev cmake unzip pkg-config git \
    rapidjson-dev python-dev libxml2-dev libxslt-dev libscrypt-dev libssl-dev openssl libgmp3-dev autoconf libtool \
    libjsoncpp-dev libjsonrpccpp-dev libjsonrpccpp-tools ccache

# After install and link for json/json.h include dir
RUN ln -s /usr/include/jsoncpp/json /usr/include/json


RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

# Add multi-stage build for docker layer caching
FROM builder as boost-layer

ENV BOOST_VERSION="1_70_0"
RUN cd /tmp \
    && wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_${BOOST_VERSION}.tar.bz2 \
    && tar --bzip2 -xf boost_${BOOST_VERSION}.tar.bz2 \
    && export CPLUS_INCLUDE_PATH="$CPLUS_INCLUDE_PATH:/usr/include/python2.7/" \
    && cd boost_${BOOST_VERSION} \
    && ./bootstrap.sh --prefix=/usr/local \
    && ./b2 install -j $(nproc) \
    && ln -s /usr/local/lib/libboost_thread.so /usr/local/lib/libboost_thread-mt.so

FROM boost-layer as gtest-layer
ENV LD_LIBRARY_PATH /usr/local/lib/
RUN git clone https://github.com/google/googletest /tmp/gtest \
  && cd /tmp/gtest && mkdir build && cd build && cmake .. \
  && make -j $(nproc) && make install -j $(nproc)

FROM gtest-layer as rocksdb-layer
RUN wget https://github.com/facebook/rocksdb/archive/v5.14.3.zip \
    && unzip v5.14.3.zip -d /tmp \
    && cd /tmp/rocksdb-5.14.3 \
    && make shared_lib PORTABLE=1 -j $(nproc) \
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

FROM leveldb-layer as grpc-layer
ENV CC=gcc
ENV CXX=g++
ENV GRPC_VERSION="v1.19.1"
RUN cd /tmp && git clone --verbose --progress --recursive --depth 1 --jobs $(nproc) --branch ${GRPC_VERSION} https://github.com/grpc/grpc.git
RUN cd /tmp/grpc/ && git submodule update --init
ENV PROTOBUF_VERSION="v3.7.1"
RUN cd /tmp/grpc/third_party/protobuf \
  && git checkout tags/${PROTOBUF_VERSION} \
  && (git pull || true) \
  && ./autogen.sh \
  && ./configure --prefix=/usr \
  && make -j $(nproc) \
  && make install -j $(nproc)

RUN cd /tmp/grpc/third_party/cares/cares && (git pull || true)
RUN cd /tmp/grpc/third_party/boringssl && (git pull || true)
RUN cd /tmp/grpc \
  && make clean \
  && make CFLAGS='-g -O2 -w' CXXFLAGS='-g -O2 -w' -j $(nproc) \
  && make CFLAGS='-g -O2 -w' CXXFLAGS='-g -O2 -w' prefix=/usr install -j $(nproc)

FROM grpc-layer as release

# Clean up image
WORKDIR /app
RUN cd / && rm -rf /tmp/*
