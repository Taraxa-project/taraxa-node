ARG BASE_IMAGE=gcr.io/jovial-meridian-249123/taraxa-node-base:latest
FROM $BASE_IMAGE as builder
ARG DEBUG=1
ARG RUN_TESTS=1
COPY . .
RUN \
if [ $RUN_TESTS -eq 1 ]; then \
    make check; \
fi
RUN \
make -j $(nproc) DEPS_INSTALL_PREFIX=/usr/local DEBUG=$DEBUG bin/main
RUN \
if [ $RUN_TESTS -eq 1 ]; then \
    make -j $(nproc) DEPS_INSTALL_PREFIX=/usr/local DEBUG=$DEBUG run_test; \
fi
RUN \
# Select binary output dir according to the build type
if [ $DEBUG -eq 1 ]; then \
    mv build/debug/bin build/bin_tmp; \
else \
    mv build/release/bin build/bin_tmp; \
fi
# 130MB -> 28MB
RUN strip build/bin_tmp/main

# Copy over only minimal information
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
COPY config ${APP_PATH}/config/

WORKDIR ${APP_PATH}
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder ${APP_PATH}/build/bin_tmp/main .
COPY --from=builder ${APP_PATH}/src/util_test/conf/*.json ./default_config/
COPY --from=builder /symlink_index.sh /
COPY --from=builder /apt_deps_runtime.txt /
# fix symlinks
RUN cd /usr/local/lib && /symlink_index.sh restore rm && rm /symlink_index.sh
RUN \
# install only runtime deps
apt-get update && \
apt-get install -y $(cat /apt_deps_runtime.txt) && \
apt-get autoclean
# Remove non-runtime stuff
RUN find /usr \
    -path '*.a' -or \
    -path '*/src' -or \
    -path '*/include' -or \
    -path '*/doc' \
    | xargs rm -rf

# Uncomment to test that all shared libraries can be loaded.
# RUN LD_BIND_NOW=1 ./main

ENV GODEBUG cgocheck=0
ENTRYPOINT ["./main"]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
