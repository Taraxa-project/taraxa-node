# Taraxa node docker environment - it contains taraxad binary + config file
#
# Build and run:
#
#   DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa/taraxa_node_dev:1.0 -f taraxa_node.dockerfile .
#   docker run -d --name taraxa_node_dev taraxa/taraxa_node_dev:1.0
#
# stop:
#   docker stop taraxa_node
#
# users:
#   root

FROM ubuntu:20.04

# Install taraxad dynamic dependencies
# all of these libs are here because of rocksdb, once rocksdb is linked statically it will be removed
RUN apt-get update \
    && apt-get install -y \
        curl \
        libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev \
        librocksdb-dev \
    && rm -rf /var/lib/apt/lists/*

# TODO: when static linking of rocksdb is done, this manual installation or ubuntu package won't be needed at all. AS tmp hack ubuntu package is faster
# Install rocksdb
# ARG ROCKSDB_VERSION=5.18.3
# RUN curl -SL https://github.com/facebook/rocksdb/archive/v$ROCKSDB_VERSION.tar.gz \
#     | tar -xzC /tmp \
#     && cd rocksdb-${ROCKSDB_VERSION} \
#     && CXXFLAGS='-Wno-error=deprecated-copy -Wno-error=pessimizing-move' make -j $(nproc) install \
#     && rm -rf $(pwd)

# Copy taraxad binary
# TODO: read 'cmake-docker-build-release' dir from taraxa.variables
RUN mkdir -p /etc/taraxad
COPY cmake-docker-build-release/install/taraxad /usr/local/bin
COPY cmake-docker-build-release/install/taraxad.conf /etc/taraxad

# Run taraxad
# ENTRYPOINT ["taraxad"]
# CMD ["--conf_taraxa", "/etc/taraxad/taraxad.conf"]
