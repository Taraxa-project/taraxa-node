FROM 541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node-base as builder

ARG go_version=1.12.1
ARG rocksdb_version=5.18.3
ENV ROCKSDB_VERSION="$rocksdb_version"

ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib
WORKDIR ${APP_PATH}

# go
RUN wget -qO- --show-progress --progress=bar:force \
    https://dl.google.com/go/go$go_version.linux-amd64.tar.gz | tar xvz -C /usr/local
ENV GOROOT=/usr/local/go
ENV GOPATH=$HOME/.go
ENV PATH=$GOPATH/bin:$GOROOT/bin:$PATH
# cmake
RUN apt-get -y install python3-pip
RUN pip3 install cmake
# rocksdb
RUN wget https://github.com/facebook/rocksdb/archive/v$rocksdb_version.zip \
    && unzip v$rocksdb_version.zip -d /tmp \
    && cd /tmp/rocksdb-$rocksdb_version \
    && make shared_lib \
    && cp librocksdb.so* /usr/local/lib \
    && cp -r ./include/* /usr/local/include
## Go rocksdb wrapper (requires at least v5.16)
#RUN CGO_CFLAGS="-I/usr/local/include" \
#    CGO_LDFLAGS="-L/usr/local/lib -lrocksdb -lstdc++ -lm -lz -lbz2 -lsnappy -llz4 -lzstd" \
#    go get github.com/tecbot/gorocksdb

ADD . .
RUN mkdir build && cd build && cmake .. && cmake --build . --target main -j $(nproc --all)
