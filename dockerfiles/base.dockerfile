FROM ubuntu:18.04 as builder

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm

RUN apt-get update \
    && apt-get install -y \
    libgflags-dev libsnappy-dev zlib1g-dev libicu-dev libbz2-dev libzstd-dev liblz4-dev git gcc-8 g++-8 clang \
    libblkid-dev e2fslibs-dev libaudit-dev wget build-essential xz-utils curl cmake unzip git \
    rapidjson-dev python-dev libxml2-dev libxslt-dev libscrypt-dev libssl-dev openssl libgmp3-dev autoconf libtool

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8    

RUN cd /tmp \
    && wget https://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.bz2 \
    && tar --bzip2 -xf boost_1_68_0.tar.bz2 \
    && export CPLUS_INCLUDE_PATH="$CPLUS_INCLUDE_PATH:/usr/include/python2.7/" \
    && cd boost_1_68_0 \
    && ./bootstrap.sh --prefix=/usr/local \
    && ./b2 install \
    && ln -s /usr/local/lib/libboost_thread.so /usr/local/lib/libboost_thread-mt.so
ENV LD_LIBRARY_PATH /usr/local/lib/

RUN git clone https://github.com/google/googletest /tmp/gtest \
  && cd /tmp/gtest && mkdir build && cd build && cmake .. && make && make install

RUN wget https://github.com/facebook/rocksdb/archive/v5.14.3.zip \
    && unzip v5.14.3.zip -d /tmp \
    && cd /tmp/rocksdb-5.14.3 \
    && make shared_lib \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include

ENV LEVELDB_VERSION="1.20"
WORKDIR /tmp
RUN wget https://github.com/google/leveldb/archive/v${LEVELDB_VERSION}.tar.gz \
  && tar xvf v${LEVELDB_VERSION}.tar.gz \
  && rm -f v${LEVELDB_VERSION}.tar.gz
WORKDIR /tmp/leveldb-${LEVELDB_VERSION}

RUN make
RUN scp -r out-static/lib* out-shared/lib* "/usr/local/lib"
RUN scp -r include/leveldb /usr/local/include
RUN ldconfig


RUN mkdir -p ${APP_PATH}
WORKDIR ${APP_PATH}
ADD . .  
RUN git submodule update --init --recursive
RUN make main

FROM ubuntu:18.04


ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib/

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/main .
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/libscrypt.* /usr/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/* /usr/lib/x86_64-linux-gnu/ 
COPY ./core_tests/*.json ./default_config/




ENTRYPOINT [ "./main" ]
CMD ["--conf_full_node", "./default_config/conf_full_node1.json", "--conf_network", "./default_config/conf_network1.json", "--conf_rpc", "./default_config/conf_rpc1.json"]

