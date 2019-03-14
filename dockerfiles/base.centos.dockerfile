FROM centos:7 as builder

# UNDER DEVELOPMENT

ENV TERM xterm
RUN yum install scl-utils centos-release-scl subscript subscription-manager -y
RUN yum groupinstall 'Development Tools' -y \
    && yum install -y \
    libgflags-dev libsnappy-dev zlib1g-dev libicu-dev libbz2-dev libzstd-dev liblz4-dev clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential xz-utils curl cmake unzip pkg-config git bzip2 \
    rapidjson-dev python-dev libxml2-dev libxslt-dev libscrypt-dev libssl-dev openssl libgmp3-dev autoconf libtool \
    which gtest gtest-devel devtoolset-8-gcc* devtoolset-8 llvm-toolset-6.0 llvm-toolset-6.0-clang-analyzer llvm-toolset-6.0-clang-tools-extra

RUN which g++

RUN echo 'source scl_source enable devtoolset-8' >> /root/.bashrc
#RUN scl enable devtoolset-8 llvm-toolset-6.0 bash

RUN yum install -y epel-release boost boost-thread boost-devel boost-regex
RUN ln -s /usr/local/lib/libboost_thread.so /usr/local/lib/libboost_thread-mt.so

ENV LD_LIBRARY_PATH /usr/local/lib/ 
WORKDIR /tmp
ENV GRPC_VERSION="v1.19.1"
RUN git clone -b $GRPC_VERSION  https://github.com/grpc/grpc 
RUN cd grpc \
  #&& source scl_source enable devtoolset-8 \
  && sed -i -e 's/cares-1_12_0/cares-1_15_0/g' .gitmodules \
  && git submodule update --init \
  && cd /tmp/grpc/third_party/protobuf \
  && ./autogen.sh \
  && ./configure --prefix=/usr/local \
  && make -j `nproc` \
  && make install \
  && cd /tmp/grpc \
  && make -j `nproc` PROTOC=/usr/local/bin/protoc \
  && make prefix=/usr/local install

RUN wget https://github.com/facebook/rocksdb/archive/v5.14.3.zip \
    && unzip v5.14.3.zip -d /tmp \
    && cd /tmp/rocksdb-5.14.3 \
   # && source scl_source enable devtoolset-8 \
    && make shared_lib \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include

ENV LEVELDB_VERSION="1.20"
WORKDIR /tmp
RUN wget https://github.com/google/leveldb/archive/v${LEVELDB_VERSION}.tar.gz \
  && tar xvf v${LEVELDB_VERSION}.tar.gz 
RUN cd /tmp/leveldb-${LEVELDB_VERSION} \
  && source scl_source enable devtoolset-8 \ 
  && make \
  && scp -r out-static/lib* out-shared/lib* "/usr/local/lib" \
  && scp -r include/leveldb /usr/local/include \
  && ldconfig      


RUN which g++