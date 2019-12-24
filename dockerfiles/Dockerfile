ARG BASE_IMAGE=gcr.io/jovial-meridian-249123/taraxa-node-base:latest
FROM $BASE_IMAGE as builder

COPY . .

RUN make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=1 main

ARG RUN_TESTS=1
RUN if [ $RUN_TESTS -eq 1 ]; then \
    make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) DEBUG=1 run_test; \
    fi

RUN make DEPS_INSTALL_PREFIX=${APP_LIBRARY_HOME} -j $(nproc) main


# Copy over only minimal information
FROM ubuntu:18.10
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/release/bin/main .
COPY --from=builder ${APP_PATH}/build/debug/bin/main ./main-d
COPY --from=builder ${APP_PATH}/scripts/docker-entrypoint.sh ./
COPY --from=builder ./core_tests/conf/*.json ./default_config/
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/* /usr/lib/

ENV DEBUG 0
ENTRYPOINT [ "./docker-entrypoint.sh" ]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
