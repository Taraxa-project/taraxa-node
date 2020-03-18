ARG BASE_IMAGE=gcr.io/jovial-meridian-249123/taraxa-node-base:latest
FROM $BASE_IMAGE as builder

COPY . .

RUN make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=1 main

ARG RUN_TESTS=1
RUN if [ $RUN_TESTS -eq 1 ]; then \
    make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=1 run_test; \
    fi

RUN make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) main

# Remove .a files to avoid it ben copied at final image
RUN find /usr/ -iname '*.a' -delete

# HACK: Remove all .so symlinks
RUN find -L /usr/local/lib/ -xtype l -iname '*.so*' -delete
# HACK: Remove duplicated librocksdb
RUN rm -rf /usr/local/lib/librocksdb.so.*

# Copy over only minimal information
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/release/bin/main .
COPY --from=builder ${APP_PATH}/build/debug/bin/main ./main-d
COPY --from=builder ${APP_PATH}/scripts/docker-entrypoint.sh ./
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/* /usr/lib/
COPY ./core_tests/conf/*.json ./default_config/
# Coping this so crypto_test can work
# this will be removed once we have a dedicated key gen
COPY --from=builder ${APP_PATH}/build/debug/bin/core_tests/crypto_test .
COPY core_tests ./core_tests

# HACK: Because we removed symlinks before
RUN ldconfig

ENV DEBUG 0
ENTRYPOINT [ "./docker-entrypoint.sh" ]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
