FROM 541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node-base:vm-integration as builder

ARG go_version=1.12.1
# Go RocksDB wrapper requires at least v5.16
ARG rocksdb_version=5.18.3

ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib
WORKDIR ${APP_PATH}
# TODO move deps to the base image
# Cmake
RUN apt-get -y install python3-pip
RUN pip3 install cmake
# Go
RUN wget -qO- --show-progress --progress=bar:force \
    https://dl.google.com/go/go$go_version.linux-amd64.tar.gz | tar xvz -C /usr/local
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
# RocksDB
RUN wget https://github.com/facebook/rocksdb/archive/v$rocksdb_version.zip \
    && unzip v$rocksdb_version.zip -d /tmp \
    && cd /tmp/rocksdb-$rocksdb_version \
    && make shared_lib -j `nproc --all` \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include
# Main build
ADD . .
RUN mkdir build && cd build && cmake .. && cmake --build . --target main -j `nproc --all`

FROM ubuntu:18.10

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/main .
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/* /usr/lib/
COPY ./core_tests/*.json ./default_config/

ENTRYPOINT [ "./main" ]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]