ARG BUILD_OUTPUT_DIR=cmake-docker-build-debug

###################################################################
# Build stage - use builder image for actual build of taraxa node #
###################################################################
FROM builder as build

# Default output dir containing build artifacts
ARG BUILD_OUTPUT_DIR

# Build taraxa-node project
WORKDIR /opt/taraxa/
COPY . .

RUN mkdir $BUILD_OUTPUT_DIR && cd $BUILD_OUTPUT_DIR \
    && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DTARAXA_ENABLE_LTO=OFF \
    -DTARAXA_STATIC_BUILD=OFF \
    ../

RUN cd $BUILD_OUTPUT_DIR && make -j$(nproc) all \
    # Copy CMake generated Testfile to be able to trigger ctest from bin directory
    && cp tests/CTestTestfile.cmake bin/ \
    # keep only required shared libraries and final binaries
    && find . -maxdepth 1 ! -name "lib" ! -name "bin" -exec rm -rfv {} \;

###############################################################################
##### Taraxa image containing taraxad binary + dynamic libraries + config #####
###############################################################################
FROM ubuntu:22.04

# Install curl and jq
RUN apt-get update \
    && apt-get install -y curl jq python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install required Python packages
RUN pip3 install click eth-account eth-utils

ARG BUILD_OUTPUT_DIR
WORKDIR /root/.taraxa

# Copy required binaries
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin/taraxad /usr/local/bin/taraxad
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/bin/taraxa-bootnode /usr/local/bin/taraxa-bootnode
COPY --from=build /opt/taraxa/$BUILD_OUTPUT_DIR/lib/*.so /usr/local/lib/

# Copy scripts
COPY scripts/taraxa-sign.py /usr/local/bin/taraxa-sign

# Set LD_LIBRARY_PATH so taraxad binary finds shared libs
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

COPY docker-entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
