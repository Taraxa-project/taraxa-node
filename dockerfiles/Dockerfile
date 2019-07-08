ARG BASE_IMAGE=541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node-base
FROM $BASE_IMAGE as builder
#FROM 541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node-base as builder
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib
WORKDIR ${APP_PATH}
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
COPY ./core_tests/conf/*.json ./default_config/

ENTRYPOINT [ "./main" ]
CMD ["--conf_taraxa", "./default_config/conf_taraxa1.json"]
