ARG BASE_IMAGE=gcr.io/jovial-meridian-249123/taraxa-node-base:latest
FROM $BASE_IMAGE as builder

ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib
WORKDIR ${APP_PATH}
ADD . .
RUN make -j 4 main
RUN make -j 4 DEBUG=1 main


# Copy over only minimal information
FROM ubuntu:18.10
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/main .
COPY --from=builder ${APP_PATH}/build-d/main ./main-d
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/* /usr/lib/
COPY ./core_tests/conf/*.json ./default_config/
COPY ./scripts/docker-entrypoint.sh ./

ENV DEBUG 0
ENTRYPOINT [ "./docker-entrypoint.sh" ]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
