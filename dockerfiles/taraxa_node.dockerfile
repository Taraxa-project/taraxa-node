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
#   root, passwd: root

FROM ubuntu:20.04

# Install standard packages
RUN apt-get update

# TODO: The goal is to have only taraxad binary with config file in this image -> all deps will be linked statically.

# Install taraxad dynamic dependencies
#TODO: link everything statically during build
RUN apt-get install -y \
    libboost-program-options-dev libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-log-dev \
    libssl-dev \
    libjsoncpp-dev libjsonrpccpp-dev \
    libscrypt-dev \
    librocksdb-dev \
    libmpfr-dev \
    libgmp3-dev
RUN apt-get clean

# TODO: read 'cmake-docker-build-release' dir from taraxa.variables
#TODO: link everything statically during build
COPY cmake-docker-build-release/submodules/lib/ /usr/local/lib
ENV LD_LIBRARY_PATH=/usr/local/lib

# Copy taraxad binary
COPY cmake-docker-build-release/src/taraxad/taraxad /usr/local/bin

# Copy taraxad config
RUN mkdir -p /etc/taraxad
# TODO: temporary hack -> scripts/taraxad.conf is hardlink to the src/tarxad/configs/taraxad.conf
COPY scripts/taraxad.conf /etc/taraxad

# Run taraxad
ENTRYPOINT ["taraxad"]
CMD ["--conf_taraxa", "etc/taraxad/taraxad.conf"]
