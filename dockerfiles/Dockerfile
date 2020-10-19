ARG BASE_IMAGE=gcr.io/jovial-meridian-249123/taraxa-node-base:latest
FROM $BASE_IMAGE as builder
ARG DEBUG=1
ARG RUN_TESTS=1

COPY . .
RUN make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=$DEBUG main crypto_test
RUN \
if [ $RUN_TESTS -eq 1 ]; then \
    make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=$DEBUG run_test; \
fi
# Remove .a files to avoid it ben copied at final image
RUN find /usr/ -iname '*.a' -delete
# HACK: Remove all .so symlinks
RUN find -L /usr/local/lib/ -xtype l -iname '*.so*' -delete
# HACK: Remove duplicated librocksdb
RUN rm -rf /usr/local/lib/librocksdb.so.*
# Copy over only minimal information
RUN \
if [ $DEBUG -eq 1 ]; then \
    mv build/debug/bin build/bin_tmp; \
else \
    mv build/release/bin build/bin_tmp; \
fi

FROM ubuntu:20.04
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/bin_tmp/main .
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/* /usr/lib/
COPY ./core_tests/conf/*.json ./default_config/
# Coping this so crypto_test can work
# this will be removed once we have a dedicated key gen
COPY --from=builder ${APP_PATH}/build/bin_tmp/core_tests/crypto_test .
COPY core_tests ./core_tests

# HACK: Because we removed symlinks before
RUN ldconfig

ENV GODEBUG cgocheck=0
ENTRYPOINT ["./main"]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
